/*
 * Copyright (C) 2017-2022 Philippe Proulx <eepp.ca>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef _YACTFR_VM_HPP
#define _YACTFR_VM_HPP

#include <cassert>
#include <string>
#include <stdexcept>
#include <vector>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <cstdint>

#include <yactfr/aliases.hpp>
#include <yactfr/elem.hpp>
#include <yactfr/data-src-factory.hpp>
#include <yactfr/elem.hpp>
#include <yactfr/elem-seq-it.hpp>
#include <yactfr/decoding-errors.hpp>

#include "proc.hpp"
#include "std-int-reader.hpp"

namespace yactfr {
namespace internal {

constexpr auto SIZE_UNSET = std::numeric_limits<Size>::max();
constexpr auto SAVED_VAL_UNSET = std::numeric_limits<std::uint64_t>::max();

// possible VM states
enum class VmState {
    BEGIN_PKT,
    BEGIN_PKT_CONTENT,
    END_PKT_CONTENT,
    END_PKT,
    BEGIN_ER,
    END_ER,
    EXEC_INSTR,
    EXEC_ARRAY_INSTR,
    READ_UUID_BYTE,
    READ_SUBSTR_UNTIL_NULL,
    READ_SUBSTR,
    END_STR,
    SET_TRACE_TYPE_UUID,
    CONTINUE_SKIP_PADDING_BITS,
    CONTINUE_SKIP_CONTENT_PADDING_BITS,
};

// VM stack frame
struct VmStackFrame final
{
    explicit VmStackFrame(const Proc& proc, const VmState parentState) :
        proc {&proc.rawProc()},
        it {proc.rawProc().begin()},
        parentState {parentState}
    {
    }

    // base procedure (container of `it` below)
    const Proc::Raw *proc;

    // _next_ instruction to execute (part of `*proc` above)
    Proc::RawIt it;

    // state when this frame was created
    VmState parentState;

    /*
     * Array elements left to read (`*proc` is the procedure of this
     * array read instruction in this case).
     */
    Size remElems;
};

/*
 * This contains the whole state of a yactfr VM _except_ for everything
 * related to data source/buffering.
 */
class VmPos final
{
public:
    explicit VmPos(const PktProc& pktProc);
    VmPos(const VmPos& other);
    VmPos& operator=(const VmPos& other);

    void state(const VmState newState) noexcept
    {
        theState = newState;
    }

    VmState state() const noexcept
    {
        return theState;
    }

    void stackPush(const Proc& proc)
    {
        stack.push_back(VmStackFrame {proc, theState});
    }

    VmStackFrame& stackTop() noexcept
    {
        assert(!stack.empty());
        return stack.back();
    }

    void stackPop()
    {
        assert(!stack.empty());
        stack.pop_back();
    }

    void setParentStateAndStackPop() noexcept
    {
        assert(!stack.empty());
        theState = this->stackTop().parentState;
        this->stackPop();
    }

    void gotoNextInstr()
    {
        ++this->stackTop().it;
    }

    void gotoNextArrayElemInstr()
    {
        auto& stackTop = this->stackTop();

        ++stackTop.it;

        if (stackTop.it == stackTop.proc->end()) {
            assert(stackTop.remElems > 0);
            --stackTop.remElems;
            stackTop.it = stackTop.proc->begin();
        }
    }

    void loadNewProc(const Proc& proc)
    {
        assert(stack.empty());
        this->stackPush(proc);
    }

    const Instr& nextInstr() noexcept
    {
        return **this->stackTop().it;
    }

    void saveVal(const Index pos) noexcept
    {
        assert(pos < savedVals.size());
        savedVals[pos] = lastIntVal.u;
    }

    std::uint64_t savedVal(const Index pos) noexcept
    {
        assert(pos < savedVals.size());
        return savedVals[pos];
    }

    std::uint64_t updateDefClkVal(const Size len) noexcept
    {
        /*
         * Special case for a 64-bit new value, which is the limit of a
         * clock value as of this version: overwrite the current value
         * directly.
         */
        if (len == 64) {
            defClkVal = lastIntVal.u;
            return lastIntVal.u;
        }

        auto curVal = defClkVal;
        const auto newValMask = (UINT64_C(1) << len) - 1;
        const auto curValMasked = curVal & newValMask;

        if (lastIntVal.u < curValMasked) {
            /*
             * It looks like a wrap occured on the number of bits of the
             * new value. Assume that the clock value wrapped only one
             * time.
             */
            curVal += newValMask + 1;
        }

        // clear the low bits of the current clock value
        curVal &= ~newValMask;

        // set the low bits of the current clock value
        curVal |= lastIntVal.u;

        // store this result
        defClkVal = curVal;
        return curVal;
    }

    Size remContentBitsInPkt() const noexcept
    {
        return curExpectedPktContentLenBits - headOffsetInCurPktBits;
    }

    Index headOffsetInElemSeqBits() const noexcept
    {
        return curPktOffsetInElemSeqBits + headOffsetInCurPktBits;
    }

    void resetForNewPkt()
    {
        headOffsetInCurPktBits = 0;
        theState = VmState::BEGIN_PKT;
        lastBo = boost::none;
        curDsPktProc = nullptr;
        curErProc = nullptr;
        curExpectedPktTotalLenBits = SIZE_UNSET;
        curExpectedPktContentLenBits = SIZE_UNSET;
        stack.clear();
        defClkVal = 0;
        std::fill(savedVals.begin(), savedVals.end(), SAVED_VAL_UNSET);
    }

private:
    void _initVectorsFromPktProc();
    void _setSimpleFromOther(const VmPos& other);
    void _setFromOther(const VmPos& other);

public:
    // offset of current packet beginning within its element sequence (bits)
    Index curPktOffsetInElemSeqBits = 0;

    // head offset within current packet (bits)
    Index headOffsetInCurPktBits = 0;

    // current elements
    struct {
        PacketBeginningElement pktBeginning;
        EndElement end;
        ScopeBeginningElement scopeBeginning;
        PacketContentBeginningElement pktContentBeginning;
        EventRecordBeginningElement erBeginning;
        DataStreamIdElement dsId;
        PacketOriginIndexElement pktOriginIndex;
        ExpectedPacketTotalLengthElement expectedPktTotalLen;
        ExpectedPacketContentLengthElement expectedPktContentLen;
        PacketMagicNumberElement pktMagicNumber;
        TraceTypeUuidElement traceTypeUuid;
        DefaultClockValueElement defClkVal;
        PacketEndDefaultClockValueElement pktEndDefClkVal;
        DataStreamTypeElement dst;
        EventRecordTypeElement ert;
        SignedIntegerElement sInt;
        UnsignedIntegerElement uInt;
        SignedEnumerationElement sEnum;
        UnsignedEnumerationElement uEnum;
        FloatingPointNumberElement flt;
        StringBeginningElement strBeginning;
        SubstringElement substr;
        StaticArrayBeginningElement staticArrayBeginning;
        StaticTextArrayBeginningElement staticTextArrayBeginning;
        DynamicArrayBeginningElement dynArrayBeginning;
        DynamicTextArrayBeginningElement dynTextArrayBeginning;
        StructureBeginningElement structBeginning;
        VariantWithSignedSelectorBeginningElement varSSelBeginning;
        VariantWithUnsignedSelectorBeginningElement varUSelBeginning;
    } elems;

    // next state to handle
    VmState theState = VmState::BEGIN_PKT;

    // state after aligning
    VmState postSkipBitsState;

    // state after reading string (until null)
    VmState postEndStrState;

    // last bit array byte order
    boost::optional<ByteOrder> lastBo;

    // remaining padding bits to skip for alignment
    Size remBitsToSkip = 0;

    // last integer value
    union {
        std::uint64_t u;
        std::int64_t i;
    } lastIntVal;

    // current ID (event record or data stream type)
    TypeId curId;

    // packet procedure
    const PktProc *pktProc = nullptr;

    // current data stream type packet procedure
    const DsPktProc *curDsPktProc = nullptr;

    // current event record type procedure
    const ErProc *curErProc = nullptr;

    // packet UUID
    boost::uuids::uuid uuid;

    // current packet expected total length (bits)
    Size curExpectedPktTotalLenBits;

    // current packet content length (bits)
    Size curExpectedPktContentLenBits;

    // stack
    std::vector<VmStackFrame> stack;

    // vector of saved values
    std::vector<std::uint64_t> savedVals;

    // default clock value, if any
    std::uint64_t defClkVal = 0;
};

class ItInfos final
{
public:
    void elemFromOther(const VmPos& myPos, const VmPos& otherPos, const Element& otherElem)
    {
        const auto otherElemAddr = reinterpret_cast<std::uintptr_t>(&otherElem);
        const auto otherPosAddr = reinterpret_cast<std::uintptr_t>(&otherPos);
        const auto diff = otherElemAddr - otherPosAddr;
        const auto myPosAddr = reinterpret_cast<std::uintptr_t>(&myPos);

        elem = reinterpret_cast<const Element *>(myPosAddr + diff);
    }

    bool operator==(const ItInfos& other) const noexcept
    {
        return offset == other.offset && mark == other.mark;
    }

    bool operator!=(const ItInfos& other) const noexcept
    {
        return offset != other.offset || mark != other.mark;
    }

    bool operator<(const ItInfos& other) const noexcept
    {
        return offset < other.offset || (offset == other.offset && mark < other.mark);
    }

    bool operator<=(const ItInfos& other) const noexcept
    {
        return offset < other.offset || (offset == other.offset && mark <= other.mark);
    }

    bool operator>(const ItInfos& other) const noexcept
    {
        return offset > other.offset || (offset == other.offset && mark > other.mark);
    }

    bool operator>=(const ItInfos& other) const noexcept
    {
        return offset > other.offset || (offset == other.offset && mark >= other.mark);
    }

public:
    Index mark = 0;
    Index offset = 0;

    /*
     * Points to one of the elements in the `elems` field of the `VmPos`
     * in the same `ElementSequenceIteratorPosition`.
     */
    const Element *elem = nullptr;
};

class Vm final
{
public:
    explicit Vm(DataSourceFactory& dataSrcFactory, const PktProc& pktProc,
                ElementSequenceIterator& it);
    Vm(const Vm& vm, ElementSequenceIterator& it);
    Vm& operator=(const Vm& vm);
    void seekPkt(Index offset);
    void savePos(ElementSequenceIteratorPosition& pos) const;
    void restorePos(const ElementSequenceIteratorPosition& pos);

    const VmPos& pos() const
    {
        return _pos;
    }

    void nextElem()
    {
        while (!this->_handleState());
    }

    void updateItElemFromOtherPos(const VmPos& otherPos, const Element * const otherElem)
    {
        if (!otherElem) {
            _it->_curElem = nullptr;
        } else {
            const auto posElemAddr = reinterpret_cast<std::uintptr_t>(otherElem);
            const auto posAddr = reinterpret_cast<std::uintptr_t>(&otherPos);
            const auto diff = posElemAddr - posAddr;
            const auto myPosAddr = reinterpret_cast<std::uintptr_t>(&_pos);

            _it->_curElem = reinterpret_cast<const Element *>(myPosAddr + diff);
        }
    }

    void it(ElementSequenceIterator& it)
    {
        _it = &it;
    }

    ElementSequenceIterator& it()
    {
        return *_it;
    }

private:
    // instruction handler reaction
    enum class _ExecReaction {
        EXEC_NEXT_INSTR,
        FETCH_NEXT_INSTR_AND_STOP,
        CHANGE_STATE,
        EXEC_CUR_INSTR,
        STOP,
    };

private:
    void _initExecFuncs();
    bool _newDataBlock(Index offsetInElemSeqBytes, Size sizeBytes);

    bool _handleState()
    {
        switch (_pos.state()) {
        case VmState::EXEC_INSTR:
            return this->_stateExecInstr();

        case VmState::EXEC_ARRAY_INSTR:
            return this->_stateExecArrayInstr();

        case VmState::BEGIN_ER:
            return this->_stateBeginEr();

        case VmState::END_ER:
            return this->_stateEndEr();

        case VmState::READ_SUBSTR:
            return this->_stateReadSubstr();

        case VmState::READ_SUBSTR_UNTIL_NULL:
            return this->_stateReadSubstrUntilNull();

        case VmState::END_STR:
            return this->_stateEndStr();

        case VmState::CONTINUE_SKIP_PADDING_BITS:
            return this->_stateContinueSkipPaddingBits();

        case VmState::CONTINUE_SKIP_CONTENT_PADDING_BITS:
            return this->_stateContinueSkipPaddingBits();

        case VmState::READ_UUID_BYTE:
            return this->_stateReadUuidByte();

        case VmState::SET_TRACE_TYPE_UUID:
            return this->_stateSetPktUuid();

        case VmState::BEGIN_PKT:
            return this->_stateBeginPkt();

        case VmState::BEGIN_PKT_CONTENT:
            return this->_stateBeginPktContent();

        case VmState::END_PKT_CONTENT:
            return this->_stateEndPktContent();

        case VmState::END_PKT:
            return this->_stateEndPkt();

        default:
            std::abort();
        }
    }

    bool _stateExecInstr()
    {
        while (true) {
            const auto reaction = this->_exec(_pos.nextInstr());

            switch (reaction) {
            case _ExecReaction::FETCH_NEXT_INSTR_AND_STOP:
                _pos.gotoNextInstr();
                return true;

            case _ExecReaction::STOP:
                return true;

            case _ExecReaction::EXEC_NEXT_INSTR:
                _pos.gotoNextInstr();
                break;

            case _ExecReaction::EXEC_CUR_INSTR:
                break;

            case _ExecReaction::CHANGE_STATE:
                // the handler changed the state: return `false` to continue
                return false;

            default:
                std::abort();
            }
        }

        return true;
    }

    bool _stateExecArrayInstr()
    {
        if (_pos.stackTop().remElems == 0) {
            _pos.setParentStateAndStackPop();
            return false;
        }

        while (true) {
            auto& stackTop = _pos.stackTop();

            if (stackTop.it == stackTop.proc->end()) {
                assert(stackTop.remElems > 0);
                --stackTop.remElems;

                if (_pos.stackTop().remElems == 0) {
                    _pos.setParentStateAndStackPop();
                    return false;
                }

                stackTop.it = stackTop.proc->begin();
                continue;
            }

            const auto reaction = this->_exec(_pos.nextInstr());

            switch (reaction) {
            case _ExecReaction::FETCH_NEXT_INSTR_AND_STOP:
                _pos.gotoNextInstr();
                return true;

            case _ExecReaction::STOP:
                return true;

            case _ExecReaction::EXEC_NEXT_INSTR:
                _pos.gotoNextInstr();
                break;

            default:
                std::abort();
            }
        }

        return true;
    }

    bool _stateContinueSkipPaddingBits()
    {
        this->_continueSkipPaddingBits(_pos.state() == VmState::CONTINUE_SKIP_CONTENT_PADDING_BITS);
        _pos.state(_pos.postSkipBitsState);

        // not done: handle next state immediately
        return false;
    }

    bool _stateBeginPkt()
    {
        this->_resetItMark();
        _pos.resetForNewPkt();

        if (this->_remBitsInBuf() == 0) {
            /*
             * Try getting 1 bit to see if we're at the end of the
             * element sequence.
             */
            if (!this->_tryHaveBits(1)) {
                this->_setItEnd();
                return true;
            }
        }

        this->_updateItCurOffset(_pos.elems.pktBeginning);
        _pos.loadNewProc(_pos.pktProc->preambleProc());
        _pos.state(VmState::BEGIN_PKT_CONTENT);
        return true;
    }

    bool _stateBeginPktContent()
    {
        this->_updateItCurOffset(_pos.elems.pktContentBeginning);

        // the packet's preamble procedure is already loaded at this point
        _pos.state(VmState::EXEC_INSTR);
        return true;
    }

    bool _stateEndPktContent()
    {
        /*
         * Next time, skip the padding bits after the packet content
         * before setting the state to `END_PKT`.
         *
         * If we have no packet total length, then the element sequence
         * contains only one packet and there's no padding after the
         * packet content.
         */
        Size bitsToSkip = 0;

        if (_pos.curExpectedPktTotalLenBits != SIZE_UNSET) {
            bitsToSkip = _pos.curExpectedPktTotalLenBits - _pos.headOffsetInCurPktBits;
        }

        if (bitsToSkip > 0) {
            _pos.remBitsToSkip = bitsToSkip;
            _pos.postSkipBitsState = VmState::END_PKT;
            _pos.state(VmState::CONTINUE_SKIP_PADDING_BITS);
        } else {
            // nothing to skip, go to end directly
            _pos.state(VmState::END_PKT);
        }

        this->_updateItCurOffset(_pos.elems.end);
        return true;
    }

    bool _stateEndPkt()
    {
        const auto offset = _pos.headOffsetInElemSeqBits();

        // readjust buffer address and offsets
        _pos.curPktOffsetInElemSeqBits = _pos.headOffsetInElemSeqBits();
        _pos.headOffsetInCurPktBits = 0;
        assert((_pos.curPktOffsetInElemSeqBits & 7) == 0);

        if (_pos.curExpectedPktTotalLenBits == SIZE_UNSET) {
            // element sequence contains a single packet
            this->_resetBuffer();
        } else {
            const auto oldBufAddr = _bufAddr;

            _bufAddr -= (_bufOffsetInCurPktBits / 8);
            _bufAddr += (_pos.curExpectedPktTotalLenBits / 8);
            _bufOffsetInCurPktBits = 0;
            _bufLenBits -= (_bufAddr - oldBufAddr) * 8;
        }

        this->_updateIt(_pos.elems.end, offset);
        _pos.state(VmState::BEGIN_PKT);
        return true;
    }

    bool _stateBeginEr()
    {
        assert(_pos.curDsPktProc);

        if (_pos.curExpectedPktContentLenBits == SIZE_UNSET) {
            if (this->_remBitsInBuf() == 0) {
                /*
                 * Try getting 1 bit to see if we're at the end of the
                 * packet.
                 */
                if (!this->_tryHaveBits(1)) {
                    _pos.state(VmState::END_PKT_CONTENT);
                    return false;
                }
            }
        } else {
            if (_pos.remContentBitsInPkt() == 0) {
                _pos.state(VmState::END_PKT_CONTENT);
                return false;
            }
        }

        // align now so that the iterator's offset is after any padding
        this->_alignHead(_pos.curDsPktProc->erAlign());

        this->_updateItCurOffset(_pos.elems.erBeginning);
        _pos.loadNewProc(_pos.curDsPktProc->erPreambleProc());
        _pos.state(VmState::EXEC_INSTR);
        return true;
    }

    bool _stateEndEr()
    {
        assert(_pos.curErProc);
        _pos.curErProc = nullptr;
        this->_updateItCurOffset(_pos.elems.end);
        _pos.state(VmState::BEGIN_ER);
        return true;
    }

    bool _stateReadUuidByte()
    {
        if (_pos.stackTop().remElems == 0) {
            // set completed UUID
            _pos.state(VmState::SET_TRACE_TYPE_UUID);
            return false;
        }

        auto& instr = **_pos.stackTop().it;

        this->_execReadStdInt<std::uint64_t, 8, readUInt8>(instr);
        _pos.uuid.data[16 - _pos.stackTop().remElems] = static_cast<std::uint8_t>(_pos.lastIntVal.u);
        --_pos.stackTop().remElems;
        return true;
    }

    bool _stateSetPktUuid()
    {
        assert(_pos.pktProc->traceType().uuid());

        // `_pos.elems.traceTypeUuid._expectedUuid` is already set once
        _pos.elems.traceTypeUuid._uuid = _pos.uuid;
        this->_updateItCurOffset(_pos.elems.traceTypeUuid);
        _pos.setParentStateAndStackPop();
        return true;
    }

    bool _stateReadSubstr()
    {
        assert((_pos.headOffsetInCurPktBits & 7) == 0);

        if (_pos.stackTop().remElems == 0) {
            _pos.setParentStateAndStackPop();
            return false;
        }

        // require at least one byte
        this->_requireContentBits(8);

        const auto buf = this->_bufAtHead();
        const auto bufSizeBytes = this->_remBitsInBuf() / 8;
        const auto substrSizeBytes = std::min(bufSizeBytes, _pos.stackTop().remElems);
        const auto substrLenBits = substrSizeBytes * 8;

        if (substrLenBits > _pos.remContentBitsInPkt()) {
            throw CannotDecodeDataBeyondPacketContentDecodingError {
                _pos.headOffsetInElemSeqBits(),
                substrLenBits, _pos.remContentBitsInPkt()
            };
        }

        _pos.elems.substr._begin = reinterpret_cast<const char *>(buf);
        _pos.elems.substr._end = reinterpret_cast<const char *>(buf + substrSizeBytes);
        assert(_pos.elems.substr.size() > 0);
        this->_updateItCurOffset(_pos.elems.substr);
        this->_consumeExistingBits(substrSizeBytes * 8);
        _pos.stackTop().remElems -= substrSizeBytes;
        return true;
    }

    bool _stateReadSubstrUntilNull()
    {
        assert((_pos.headOffsetInCurPktBits & 7) == 0);

        // require at least one byte
        this->_requireContentBits(8);

        const auto buf = this->_bufAtHead();
        const auto bufSizeBytes = this->_remBitsInBuf() / 8;

        assert(bufSizeBytes >= 1);

        auto res = reinterpret_cast<const char *>(std::memchr(buf, 0, bufSizeBytes));
        auto begin = reinterpret_cast<const char *>(buf);
        const char *end;

        if (res) {
            // _after_ the null byte to include it
            end = res + 1;
        } else {
            // no null byte yet: current end of buffer
            end = reinterpret_cast<const char *>(buf + bufSizeBytes);
        }

        const Size substrLenBits = (end - begin) * 8;

        if (substrLenBits > _pos.remContentBitsInPkt()) {
            throw CannotDecodeDataBeyondPacketContentDecodingError {
                _pos.headOffsetInElemSeqBits(),
                substrLenBits, _pos.remContentBitsInPkt()
            };
        }

        _pos.elems.substr._begin = begin;
        _pos.elems.substr._end = end;

        if (res) {
            // we're done
            _pos.state(VmState::END_STR);
        }

        assert(_pos.elems.substr.size() > 0);
        this->_updateItCurOffset(_pos.elems.substr);
        this->_consumeExistingBits(_pos.elems.substr.size() * 8);
        return true;
    }

    bool _stateEndStr()
    {
        this->_updateItCurOffset(_pos.elems.end);
        _pos.state(_pos.postEndStrState);
        assert(_pos.state() == VmState::EXEC_INSTR || _pos.state() == VmState::EXEC_ARRAY_INSTR);
        return true;
    }

    _ExecReaction _exec(const Instr& instr)
    {
        return (this->*_execFuncs[static_cast<Index>(instr.kind())])(instr);
    }

    void _updateIt(const Element& elem, const Index offset)
    {
        _it->_curElem = &elem;
        _it->_offset = offset;
        ++_it->_mark;
    }

    void _updateItCurOffset(const Element& elem)
    {
        _it->_curElem = &elem;
        _it->_offset = _pos.headOffsetInElemSeqBits();
        ++_it->_mark;
    }

    void _setItEnd() const noexcept
    {
        _it->_mark = 0;
        _it->_offset = ElementSequenceIterator::_END_OFFSET;
    }

    void _resetItMark() const noexcept
    {
        _it->_mark = 0;
    }

    void _alignHead(const Size align)
    {
        const auto newHeadOffsetBits = (_pos.headOffsetInCurPktBits + align - 1) & -align;
        const auto bitsToSkip = newHeadOffsetBits - _pos.headOffsetInCurPktBits;

        if (bitsToSkip == 0) {
            // already aligned! yay!
            return;
        }

        if (bitsToSkip > _pos.remContentBitsInPkt()) {
            throw CannotDecodeDataBeyondPacketContentDecodingError {
                _pos.headOffsetInElemSeqBits(),
                bitsToSkip, _pos.remContentBitsInPkt()
            };
        }

        _pos.remBitsToSkip = bitsToSkip;
        _pos.postSkipBitsState = _pos.state();
        _pos.state(VmState::CONTINUE_SKIP_CONTENT_PADDING_BITS);
        this->_continueSkipPaddingBits(true);
    }

    void _alignHead(const Instr& instr)
    {
        this->_alignHead(static_cast<const ReadDataInstr&>(instr).align());
    }

    void _continueSkipPaddingBits(const bool contentBits)
    {
        while (_pos.remBitsToSkip > 0) {
            if (contentBits) {
                this->_requireContentBits(1);
            } else {
                this->_requireBits(1);
            }

            const auto bitsToSkip = std::min(_pos.remBitsToSkip, this->_remBitsInBuf());

            _pos.remBitsToSkip -= bitsToSkip;
            this->_consumeExistingBits(bitsToSkip);
        }

        // we're done now!
        _pos.state(_pos.postSkipBitsState);
    }

    bool _tryHaveBits(const Size bits)
    {
        assert(bits <= 64);

        if (bits <= this->_remBitsInBuf()) {
            // we still have enough
            return true;
        }

        /*
         * Align the current head to its current byte and compute the
         * offset, from the beginning of the element sequence, to
         * request in bytes at this point.
         */
        const auto flooredHeadOffsetInCurPacketBits = _pos.headOffsetInCurPktBits & ~7ULL;
        const auto flooredHeadOffsetInCurPacketBytes = flooredHeadOffsetInCurPacketBits / 8;
        const auto curPacketOffsetInElemSeqBytes = _pos.curPktOffsetInElemSeqBits / 8;
        const auto requestOffsetInElemSeqBytes = curPacketOffsetInElemSeqBytes +
                                                 flooredHeadOffsetInCurPacketBytes;
        const auto bitInByte = _pos.headOffsetInCurPktBits & 7;
        const auto sizeBytes = (bits + 7 + bitInByte) / 8;

        return this->_newDataBlock(requestOffsetInElemSeqBytes, sizeBytes);
    }

    void _requireBits(const Size bits)
    {
        if (!this->_tryHaveBits(bits)) {
            throw PrematureEndOfDataDecodingError {
                _pos.headOffsetInElemSeqBits(), bits
            };
        }
    }

    void _requireContentBits(const Size bits)
    {
        if (bits > _pos.remContentBitsInPkt()) {
            // going past the packet's content
            throw CannotDecodeDataBeyondPacketContentDecodingError {
                _pos.headOffsetInElemSeqBits(),
                bits, _pos.remContentBitsInPkt()
            };
        }

        this->_requireBits(bits);
    }

    const std::uint8_t *_bufAtHead() const noexcept
    {
        const auto offsetBytes = (_pos.headOffsetInCurPktBits - _bufOffsetInCurPktBits) / 8;

        return &_bufAddr[offsetBytes];
    }

    Size _remBitsInBuf() const noexcept
    {
        return (_bufOffsetInCurPktBits + _bufLenBits) - _pos.headOffsetInCurPktBits;
    }

    void _consumeExistingBits(const Size bitsToConsume) noexcept
    {
        assert(bitsToConsume <= this->_remBitsInBuf());
        _pos.headOffsetInCurPktBits += bitsToConsume;
    }

    void _resetBuffer() noexcept
    {
        _bufAddr = nullptr;
        _bufLenBits = 0;
        _bufOffsetInCurPktBits = _pos.headOffsetInCurPktBits;
    }

    // instruction handlers
    _ExecReaction _execReadSIntLe(const Instr& instr);
    _ExecReaction _execReadSIntBe(const Instr& instr);
    _ExecReaction _execReadSIntA8(const Instr& instr);
    _ExecReaction _execReadSIntA16Le(const Instr& instr);
    _ExecReaction _execReadSIntA32Le(const Instr& instr);
    _ExecReaction _execReadSIntA64Le(const Instr& instr);
    _ExecReaction _execReadSIntA16Be(const Instr& instr);
    _ExecReaction _execReadSIntA32Be(const Instr& instr);
    _ExecReaction _execReadSIntA64Be(const Instr& instr);
    _ExecReaction _execReadUIntLe(const Instr& instr);
    _ExecReaction _execReadUIntBe(const Instr& instr);
    _ExecReaction _execReadUIntA8(const Instr& instr);
    _ExecReaction _execReadUIntA16Le(const Instr& instr);
    _ExecReaction _execReadUIntA32Le(const Instr& instr);
    _ExecReaction _execReadUIntA64Le(const Instr& instr);
    _ExecReaction _execReadUIntA16Be(const Instr& instr);
    _ExecReaction _execReadUIntA32Be(const Instr& instr);
    _ExecReaction _execReadUIntA64Be(const Instr& instr);
    _ExecReaction _execReadFloat32Le(const Instr& instr);
    _ExecReaction _execReadFloat32Be(const Instr& instr);
    _ExecReaction _execReadFloatA32Le(const Instr& instr);
    _ExecReaction _execReadFloatA32Be(const Instr& instr);
    _ExecReaction _execReadFloat64Le(const Instr& instr);
    _ExecReaction _execReadFloat64Be(const Instr& instr);
    _ExecReaction _execReadFloatA64Le(const Instr& instr);
    _ExecReaction _execReadFloatA64Be(const Instr& instr);
    _ExecReaction _execReadSEnumLe(const Instr& instr);
    _ExecReaction _execReadSEnumBe(const Instr& instr);
    _ExecReaction _execReadSEnumA8(const Instr& instr);
    _ExecReaction _execReadSEnumA16Le(const Instr& instr);
    _ExecReaction _execReadSEnumA32Le(const Instr& instr);
    _ExecReaction _execReadSEnumA64Le(const Instr& instr);
    _ExecReaction _execReadSEnumA16Be(const Instr& instr);
    _ExecReaction _execReadSEnumA32Be(const Instr& instr);
    _ExecReaction _execReadSEnumA64Be(const Instr& instr);
    _ExecReaction _execReadUEnumLe(const Instr& instr);
    _ExecReaction _execReadUEnumBe(const Instr& instr);
    _ExecReaction _execReadUEnumA8(const Instr& instr);
    _ExecReaction _execReadUEnumA16Le(const Instr& instr);
    _ExecReaction _execReadUEnumA32Le(const Instr& instr);
    _ExecReaction _execReadUEnumA64Le(const Instr& instr);
    _ExecReaction _execReadUEnumA16Be(const Instr& instr);
    _ExecReaction _execReadUEnumA32Be(const Instr& instr);
    _ExecReaction _execReadUEnumA64Be(const Instr& instr);
    _ExecReaction _execReadStr(const Instr& instr);
    _ExecReaction _execBeginReadScope(const Instr& instr);
    _ExecReaction _execEndReadScope(const Instr& instr);
    _ExecReaction _execBeginReadStruct(const Instr& instr);
    _ExecReaction _execEndReadStruct(const Instr& instr);
    _ExecReaction _execBeginReadStaticArray(const Instr& instr);
    _ExecReaction _execEndReadStaticArray(const Instr& instr);
    _ExecReaction _execBeginReadStaticTextArray(const Instr& instr);
    _ExecReaction _execEndReadStaticTextArray(const Instr& instr);
    _ExecReaction _execBeginReadStaticUuidArray(const Instr& instr);
    _ExecReaction _execBeginReadDynArray(const Instr& instr);
    _ExecReaction _execEndReadDynArray(const Instr& instr);
    _ExecReaction _execBeginReadDynTextArray(const Instr& instr);
    _ExecReaction _execEndReadDynTextArray(const Instr& instr);
    _ExecReaction _execBeginReadVarSSel(const Instr& instr);
    _ExecReaction _execBeginReadVarUSel(const Instr& instr);
    _ExecReaction _execEndReadVar(const Instr& instr);
    _ExecReaction _execSaveVal(const Instr& instr);
    _ExecReaction _execSetPktEndDefClkVal(const Instr& instr);
    _ExecReaction _execUpdateDefClkVal(const Instr& instr);
    _ExecReaction _execSetCurrentId(const Instr& instr);
    _ExecReaction _execSetDst(const Instr& instr);
    _ExecReaction _execSetErt(const Instr& instr);
    _ExecReaction _execSetDsId(const Instr& instr);
    _ExecReaction _execSetPktOriginIndex(const Instr& instr);
    _ExecReaction _execSetPktTotalLen(const Instr& instr);
    _ExecReaction _execSetPktContentLen(const Instr& instr);
    _ExecReaction _execSetPktMagicNumber(const Instr& instr);
    _ExecReaction _execEndPktPreambleProc(const Instr& instr);
    _ExecReaction _execEndDsPktPreambleProc(const Instr& instr);
    _ExecReaction _execEndDsErPreambleProc(const Instr& instr);
    _ExecReaction _execEndErProc(const Instr& instr);

    static void _setDataElemFromInstr(DataElement& elem, const ReadDataInstr& readDataInstr) noexcept
    {
        elem._structMemberType = readDataInstr.memberType();
    }

    template <typename ValT, typename ElemT>
    void _setIntElemBase(const ValT val, const Instr& instr, ElemT& elem) noexcept
    {
        using DataTypeT = typename std::remove_const<typename std::remove_reference<decltype(elem.type())>::type>::type;

        auto& readDataInstr = static_cast<const ReadDataInstr&>(instr);

        Vm::_setDataElemFromInstr(elem, readDataInstr);
        elem._dt = static_cast<const DataTypeT *>(&readDataInstr.dt());

        if (std::is_signed<ValT>::value) {
            _pos.lastIntVal.i = val;
        } else {
            _pos.lastIntVal.u = val;
        }

        elem._val = val;
        this->_updateItCurOffset(elem);
    }

    template <typename ValT>
    void _setIntElem(const ValT val, const Instr& instr) noexcept
    {
        if (std::is_signed<ValT>::value) {
            this->_setIntElemBase(val, instr, _pos.elems.sInt);
        } else {
            this->_setIntElemBase(val, instr, _pos.elems.uInt);
        }
    }

    template <typename ValT>
    void _setEnumElem(const ValT val, const Instr& instr) noexcept
    {
        if (std::is_signed<ValT>::value) {
            this->_setIntElemBase(val, instr, _pos.elems.sEnum);
        } else {
            this->_setIntElemBase(val, instr, _pos.elems.uEnum);
        }
    }

    void _setFloatVal(const double val, const ReadDataInstr& instr) noexcept
    {
        Vm::_setDataElemFromInstr(_pos.elems.flt, instr);
        _pos.elems.flt._dt = static_cast<const FloatingPointNumberType *>(&instr.dt());
        _pos.elems.flt._val = val;
        this->_updateItCurOffset(_pos.elems.flt);
    }

    void _execReadBitArrayPreamble(const Instr& instr, const Size len)
    {
        auto& readBitArrayInstr = static_cast<const ReadBitArrayInstr&>(instr);

        this->_alignHead(readBitArrayInstr);
        this->_requireContentBits(len);
    }

    template <typename RetT, Size LenBits, RetT (*Func)(const std::uint8_t *)>
    RetT _readStdInt(const Instr& instr)
    {
        auto& readBitArrayInstr = static_cast<const ReadBitArrayInstr&>(instr);

        this->_execReadBitArrayPreamble(instr, LenBits);
        _pos.lastBo = readBitArrayInstr.bo();
        return Func(this->_bufAtHead());
    }

    template <typename RetT, Size LenBits, RetT (*Func)(const std::uint8_t *)>
    void _execReadStdInt(const Instr& instr)
    {
        const auto val = this->_readStdInt<RetT, LenBits, Func>(instr);

        this->_setIntElem(val, instr);
        this->_consumeExistingBits(LenBits);
    }

    template <typename RetT, Size LenBits, RetT (*Func)(const std::uint8_t *)>
    void _execReadStdEnum(const Instr& instr)
    {
        const auto val = this->_readStdInt<RetT, LenBits, Func>(instr);

        this->_setEnumElem(val, instr);
        this->_consumeExistingBits(LenBits);
    }

    template <typename RetT, RetT (*Funcs[])(const std::uint8_t *)>
    RetT _readInt(const Instr& instr)
    {
        auto& readBitArrayInstr = static_cast<const ReadBitArrayInstr&>(instr);

        this->_execReadBitArrayPreamble(instr, readBitArrayInstr.len());

        if (static_cast<bool>(_pos.lastBo)) {
            if ((_pos.headOffsetInCurPktBits & 7) != 0) {
                /*
                 * A bit array which does not start on a byte boundary
                 * must have the same byte order as the previous bit
                 * array.
                 */
                if (readBitArrayInstr.bo() != *_pos.lastBo) {
                    throw ByteOrderChangeWithinByteDecodingError {
                        _pos.headOffsetInElemSeqBits(),
                        *_pos.lastBo,
                        readBitArrayInstr.bo()
                    };
                }
            }
        }

        _pos.lastBo = readBitArrayInstr.bo();

        const auto index = (readBitArrayInstr.len() - 1) * 8 + (_pos.headOffsetInCurPktBits & 7);

        return Funcs[index](this->_bufAtHead());
    }

    template <typename RetT, RetT (*Funcs[])(const std::uint8_t *)>
    void _execReadInt(const Instr& instr)
    {
        const auto val = this->_readInt<RetT, Funcs>(instr);

        this->_setIntElem(val, instr);
        this->_consumeExistingBits(static_cast<const ReadBitArrayInstr&>(instr).len());
    }

    template <typename RetT, RetT (*Funcs[])(const std::uint8_t *)>
    void _execReadEnum(const Instr& instr)
    {
        const auto val = this->_readInt<RetT, Funcs>(instr);

        this->_setEnumElem(val, instr);
        this->_consumeExistingBits(static_cast<const ReadBitArrayInstr&>(instr).len());
    }

    template <typename FloatT>
    void _execReadFloatPost(const std::uint64_t val, const Instr& instr) noexcept
    {
        // is there a better way to do this?
        using UIntT = std::conditional_t<sizeof(FloatT) == sizeof(std::uint32_t),
                                         std::uint32_t, std::uint64_t>;

        static_assert(sizeof(FloatT) == sizeof(UIntT),
                      "Floating point number and integer sizes match in union.");
        static_assert(std::alignment_of<FloatT>::value == std::alignment_of<UIntT>::value,
                      "Floating point number and integer alignments match in union.");

        union {
            FloatT flt;
            UIntT uInt;
        } u;

        u.uInt = static_cast<UIntT>(val);
        this->_setFloatVal(u.flt, static_cast<const ReadDataInstr&>(instr));
        this->_consumeExistingBits(sizeof(FloatT) * 8);
    }

    template <typename FloatT, std::uint64_t (*Funcs[])(const std::uint8_t *)>
    void _execReadFloat(const Instr& instr)
    {
        const auto val = this->_readInt<std::uint64_t, Funcs>(instr);

        this->_execReadFloatPost<FloatT>(val, instr);
    }

    template <typename FloatT, std::uint64_t (*Func)(const std::uint8_t *)>
    void _execReadStdFloat(const Instr& instr)
    {
        const auto val = this->_readStdInt<std::uint64_t, sizeof(FloatT) * 8, Func>(instr);

        this->_execReadFloatPost<FloatT>(val, instr);
    }

    template <typename ReadVarInstrT, typename ElemT>
    void _execBeginReadVar(const Instr& instr, ElemT& elem)
    {
        this->_alignHead(instr);

        const auto& beginReadVarInstr = static_cast<const ReadVarInstrT&>(instr);
        const auto uSelVal = _pos.savedVal(beginReadVarInstr.selPos());
        const auto selVal = static_cast<typename ReadVarInstrT::Opt::Val>(uSelVal);
        const auto proc = beginReadVarInstr.procForSelVal(selVal);

        if (!proc) {
            if (std::is_signed<typename ReadVarInstrT::Opt::Val>::value) {
                throw InvalidVariantSignedSelectorValueDecodingError {
                    _pos.headOffsetInElemSeqBits(),
                    static_cast<std::int64_t>(selVal)
                };
            } else {
                throw InvalidVariantUnsignedSelectorValueDecodingError {
                    _pos.headOffsetInElemSeqBits(),
                    static_cast<std::uint64_t>(selVal)
                };
            }
        }

        Vm::_setDataElemFromInstr(elem, beginReadVarInstr);
        elem._dt = &beginReadVarInstr.varType();
        elem._selVal = selVal;
        this->_updateItCurOffset(elem);
        _pos.gotoNextInstr();
        _pos.stackPush(*proc);
        _pos.state(VmState::EXEC_INSTR);
    }

    void _execBeginReadStaticArrayCommon(const Instr& instr, StaticArrayBeginningElement& elem,
                                         const VmState nextState)
    {
        const auto& beginReadStaticArrayInstr = static_cast<const BeginReadStaticArrayInstr&>(instr);
        auto& staticArrayBeginningElem = static_cast<StaticArrayBeginningElement&>(elem);
        auto& dataElem = static_cast<DataElement&>(staticArrayBeginningElem);

        this->_alignHead(instr);
        Vm::_setDataElemFromInstr(dataElem, beginReadStaticArrayInstr);
        elem._dt = &beginReadStaticArrayInstr.staticArrayType();
        elem._len = beginReadStaticArrayInstr.staticArrayType().length();
        this->_updateItCurOffset(staticArrayBeginningElem);
        _pos.gotoNextInstr();
        _pos.stackPush(beginReadStaticArrayInstr.proc());
        _pos.stackTop().remElems = beginReadStaticArrayInstr.len();
        _pos.state(nextState);
    }

    void _execBeginReadDynArrayCommon(const Instr& instr, DynamicArrayBeginningElement& elem,
                                      const VmState nextState)
    {
        const auto& beginReadDynArrayInstr = static_cast<const BeginReadDynArrayInstr&>(instr);
        const auto len = _pos.savedVal(beginReadDynArrayInstr.lenPos());

        assert(len != SAVED_VAL_UNSET);
        this->_alignHead(instr);
        Vm::_setDataElemFromInstr(elem, beginReadDynArrayInstr);
        elem._dt = &beginReadDynArrayInstr.dynArrayType();
        elem._len = len;
        this->_updateItCurOffset(elem);
        _pos.gotoNextInstr();
        _pos.stackPush(beginReadDynArrayInstr.proc());
        _pos.stackTop().remElems = len;
        _pos.state(nextState);
    }

private:
    DataSourceFactory *_dataSrcFactory;
    DataSource::UP _dataSrc;

    // current buffer
    const std::uint8_t *_bufAddr = nullptr;

    // length of current buffer (bits)
    Size _bufLenBits = 0;

    // offset of buffer within current packet (bits)
    Index _bufOffsetInCurPktBits = 0;

    // owning element sequence iterator
    ElementSequenceIterator *_it;

    // array of instruction handler functions
    std::array<_ExecReaction (Vm::*)(const Instr&), 80> _execFuncs;

    // position (whole VM's state)
    VmPos _pos;
};

} // namespace internal
} // namespace yactfr

#endif // _YACTFR_VM_HPP
