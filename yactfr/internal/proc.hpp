/*
 * Copyright (C) 2016-2024 Philippe Proulx <eepp.ca>
 *
 * This software may be modified and distributed under the terms of the
 * MIT license. See the LICENSE file for details.
 */

/*
 * Here are the possible instructions for the yactfr VM.
 *
 * No numeric bytecode is involved here: the VM deals with a sequence of
 * procedure instruction objects, some of them also containing a
 * subprocedure, and so on.
 *
 * Some definitions:
 *
 * Procedure:
 *     A sequence of procedure instructions.
 *
 * Subprocedure:
 *     A procedure contained in a procedure instruction.
 *
 * Procedure instruction:
 *     An instruction for the yactfr VM, possibly containing one or
 *     more subprocedures.
 *
 * The top-level procedure is a `PktProc`. A `PktProc` object contains
 * all the instructions to execute for a whole packet.
 *
 * At the beginning of a packet:
 *
 * * Execute the preamble procedure of the packet procedure.
 *
 * A `DsPktProc` object contains the instructions to execute after the
 * preamble procedure of the packet procedure for any data stream of a
 * specific type.
 *
 * To execute a data stream packet procedure:
 *
 * 1. Execute the per-packet preamble procedure.
 *
 * 2. Until the end of the packet, repeat:
 *
 *    a) Execute the common event record preamble procedure.
 *
 *    b) Depending on the chosen event record type, execute the
 *       corresponding event record procedure (`ErProc`).
 *
 * An `ErProc` object contains a single procedure, that is, the
 * instructions to execute after the common event record preamble
 * procedure of its parent `DsPktProc`.
 *
 * Details such as how to choose the current data stream and event
 * record types, and how to determine the end of the packet, are left to
 * the implementation of the VM.
 */

#ifndef YACTFR_INTERNAL_PROC_HPP
#define YACTFR_INTERNAL_PROC_HPP

#include <cstdlib>
#include <cassert>
#include <sstream>
#include <list>
#include <vector>
#include <utility>
#include <functional>
#include <type_traits>
#include <boost/optional/optional.hpp>

#include <yactfr/aliases.hpp>
#include <yactfr/metadata/dt.hpp>
#include <yactfr/metadata/fl-bit-array-type.hpp>
#include <yactfr/metadata/fl-bit-map-type.hpp>
#include <yactfr/metadata/fl-bool-type.hpp>
#include <yactfr/metadata/fl-int-type.hpp>
#include <yactfr/metadata/fl-float-type.hpp>
#include <yactfr/metadata/vl-int-type.hpp>
#include <yactfr/metadata/nt-str-type.hpp>
#include <yactfr/metadata/struct-type.hpp>
#include <yactfr/metadata/sl-array-type.hpp>
#include <yactfr/metadata/dl-array-type.hpp>
#include <yactfr/metadata/sl-str-type.hpp>
#include <yactfr/metadata/dl-str-type.hpp>
#include <yactfr/metadata/sl-blob-type.hpp>
#include <yactfr/metadata/dl-blob-type.hpp>
#include <yactfr/metadata/var-type.hpp>
#include <yactfr/metadata/opt-type.hpp>
#include <yactfr/metadata/clk-type.hpp>
#include <yactfr/metadata/ert.hpp>
#include <yactfr/metadata/dst.hpp>
#include <yactfr/metadata/trace-type.hpp>

#include "utils.hpp"
#include "vendor/wise-enum/wise_enum.h"

namespace yactfr {
namespace internal {

class BeginReadDlArrayInstr;
class BeginReadDlStrInstr;
class BeginReadScopeInstr;
class BeginReadSlArrayInstr;
class BeginReadSlStrInstr;
class BeginReadSlUuidArrayInstr;
class BeginReadDlBlobInstr;
class BeginReadSlBlobInstr;
class BeginReadSlUuidBlobInstr;
class BeginReadStructInstr;
class BeginReadVarSIntSelInstr;
class BeginReadVarUIntSelInstr;
class BeginReadOptBoolSelInstr;
class BeginReadOptUIntSelInstr;
class BeginReadOptSIntSelInstr;
class DecrRemainingElemsInstr;
class EndDsErPreambleProcInstr;
class EndDsPktPreambleProcInstr;
class EndErProcInstr;
class EndPktPreambleProcInstr;
class EndReadDataInstr;
class EndReadScopeInstr;
class Instr;
class ReadDataInstr;
class ReadFlBitArrayInstr;
class ReadFlBitMapInstr;
class ReadFlBoolInstr;
class ReadFlFloatInstr;
class ReadFlSIntInstr;
class ReadNtStrInstr;
class ReadFlUIntInstr;
class ReadVlIntInstr;
class SaveValInstr;
class SetCurIdInstr;
class SetDsIdInstr;
class SetDstInstr;
class SetDsInfoInstr;
class SetErtInstr;
class SetErInfoInstr;
class SetExpectedPktContentLenInstr;
class SetPktEndDefClkValInstr;
class SetPktMagicNumberInstr;
class SetPktSeqNumInstr;
class SetPktDiscErCounterSnapInstr;
class SetExpectedPktTotalLenInstr;
class SetPktInfoInstr;
class UpdateDefClkValInstr;

/*
 * A classic abstract visitor class for procedure instructions.
 *
 * Used by `PktProcBuilder`, NOT by the VM.
 */
class InstrVisitor
{
protected:
    explicit InstrVisitor() = default;

public:
    virtual ~InstrVisitor() = default;

    virtual void visit(ReadFlBitArrayInstr&)
    {
    }

    virtual void visit(ReadFlBitMapInstr&)
    {
    }

    virtual void visit(ReadFlBoolInstr&)
    {
    }

    virtual void visit(ReadFlSIntInstr&)
    {
    }

    virtual void visit(ReadFlUIntInstr&)
    {
    }

    virtual void visit(ReadFlFloatInstr&)
    {
    }

    virtual void visit(ReadVlIntInstr&)
    {
    }

    virtual void visit(ReadNtStrInstr&)
    {
    }

    virtual void visit(BeginReadScopeInstr&)
    {
    }

    virtual void visit(EndReadScopeInstr&)
    {
    }

    virtual void visit(BeginReadStructInstr&)
    {
    }

    virtual void visit(BeginReadSlArrayInstr&)
    {
    }

    virtual void visit(BeginReadSlUuidArrayInstr&)
    {
    }

    virtual void visit(BeginReadDlArrayInstr&)
    {
    }

    virtual void visit(BeginReadSlStrInstr&)
    {
    }

    virtual void visit(BeginReadDlStrInstr&)
    {
    }

    virtual void visit(BeginReadSlBlobInstr&)
    {
    }

    virtual void visit(BeginReadSlUuidBlobInstr&)
    {
    }

    virtual void visit(BeginReadDlBlobInstr&)
    {
    }

    virtual void visit(BeginReadVarUIntSelInstr&)
    {
    }

    virtual void visit(BeginReadVarSIntSelInstr&)
    {
    }

    virtual void visit(BeginReadOptBoolSelInstr&)
    {
    }

    virtual void visit(BeginReadOptUIntSelInstr&)
    {
    }

    virtual void visit(BeginReadOptSIntSelInstr&)
    {
    }

    virtual void visit(EndReadDataInstr&)
    {
    }

    virtual void visit(UpdateDefClkValInstr&)
    {
    }

    virtual void visit(SetCurIdInstr&)
    {
    }

    virtual void visit(SetDstInstr&)
    {
    }

    virtual void visit(SetErtInstr&)
    {
    }

    virtual void visit(SetErInfoInstr&)
    {
    }

    virtual void visit(SetDsIdInstr&)
    {
    }

    virtual void visit(SetDsInfoInstr&)
    {
    }

    virtual void visit(SetPktSeqNumInstr&)
    {
    }

    virtual void visit(SetPktDiscErCounterSnapInstr&)
    {
    }

    virtual void visit(SetExpectedPktTotalLenInstr&)
    {
    }

    virtual void visit(SetExpectedPktContentLenInstr&)
    {
    }

    virtual void visit(SaveValInstr&)
    {
    }

    virtual void visit(SetPktEndDefClkValInstr&)
    {
    }

    virtual void visit(SetPktInfoInstr&)
    {
    }

    virtual void visit(SetPktMagicNumberInstr&)
    {
    }

    virtual void visit(EndPktPreambleProcInstr&)
    {
    }

    virtual void visit(EndDsPktPreambleProcInstr&)
    {
    }

    virtual void visit(EndDsErPreambleProcInstr&)
    {
    }

    virtual void visit(EndErProcInstr&)
    {
    }

    virtual void visit(DecrRemainingElemsInstr&)
    {
    }
};

/*
 * A procedure, that is, a sequence of instructions.
 *
 * The procedure is first built as a list of shared pointers because the
 * build process needs to insert and replace instructions and it's
 * easier with a linked list.
 *
 * Then, when the build is complete, we call buildRawProcFromShared()
 * which builds a vector of raw instruction object (weak) pointers from
 * the list of shared pointers. The list must remain alive as it keeps
 * the instructions alive. Going from raw pointer to raw pointer in a
 * vector seems more efficient than going from shared pointer to shared
 * pointer in a linked list. I did not measure the difference yet
 * however.
 *
 * Instructions are shared because sometimes they are reused, for
 * example multiple range procedures of a `BeginReadVarInstr`
 * instruction can refer to the exact same instructions.
 */
class Proc final
{
public:
    using Raw = std::vector<const Instr *>;
    using Shared = std::list<std::shared_ptr<Instr>>;
    using RawIt = Raw::const_iterator;
    using SharedIt = Shared::iterator;

public:
    void buildRawProcFromShared();
    std::string toStr(Size indent = 0) const;
    void pushBack(std::shared_ptr<Instr> instr);
    SharedIt insert(SharedIt it, std::shared_ptr<Instr> instr);

    Shared& sharedProc() noexcept
    {
        return _sharedProc;
    }

    const Shared& sharedProc() const noexcept
    {
        return _sharedProc;
    }

    const Raw& rawProc() const noexcept
    {
        return _rawProc;
    }

    SharedIt begin() noexcept
    {
        return _sharedProc.begin();
    }

    SharedIt end() noexcept
    {
        return _sharedProc.end();
    }

private:
    Raw _rawProc;
    Shared _sharedProc;
};

/*
 * A pair of procedure and instruction iterator.
 */
struct InstrLoc final
{
    Proc::Shared *proc = nullptr;
    Proc::Shared::iterator it;
};

/*
 * List of instruction locations.
 */
using InstrLocs = std::vector<InstrLoc>;

/*
 * Procedure instruction abstract class.
 */
class Instr
{
public:
    // kind of instruction (opcode)
    WISE_ENUM_CLASS_MEMBER((Kind, unsigned int),
        Unset,
        BeginReadDlArray,
        BeginReadDlBlob,
        BeginReadDlStr,
        BeginReadOptBoolSel,
        BeginReadOptSIntSel,
        BeginReadOptUIntSel,
        BeginReadScope,
        BeginReadSlArray,
        BeginReadSlBlob,
        BeginReadSlStr,
        BeginReadSlUuidArray,
        BeginReadSlUuidBlob,
        BeginReadStruct,
        BeginReadVarSIntSel,
        BeginReadVarUIntSel,
        DecrRemainingElems,
        EndDsErPreambleProc,
        EndDsPktPreambleProc,
        EndErProc,
        EndPktPreambleProc,
        EndReadDlArray,
        EndReadDlBlob,
        EndReadDlStr,
        EndReadOptBoolSel,
        EndReadOptSIntSel,
        EndReadOptUIntSel,
        EndReadScope,
        EndReadSlArray,
        EndReadSlBlob,
        EndReadSlStr,
        EndReadStruct,
        EndReadVarSIntSel,
        EndReadVarUIntSel,
        ReadFlBitArrayA16Be,
        ReadFlBitArrayA16BeRev,
        ReadFlBitArrayA16Le,
        ReadFlBitArrayA16LeRev,
        ReadFlBitArrayA32Be,
        ReadFlBitArrayA32BeRev,
        ReadFlBitArrayA32Le,
        ReadFlBitArrayA32LeRev,
        ReadFlBitArrayA64Be,
        ReadFlBitArrayA64BeRev,
        ReadFlBitArrayA64Le,
        ReadFlBitArrayA64LeRev,
        ReadFlBitArrayA8,
        ReadFlBitArrayA8Rev,
        ReadFlBitArrayBe,
        ReadFlBitArrayBeRev,
        ReadFlBitArrayLe,
        ReadFlBitArrayLeRev,
        ReadFlBitMapA16Be,
        ReadFlBitMapA16BeRev,
        ReadFlBitMapA16Le,
        ReadFlBitMapA16LeRev,
        ReadFlBitMapA32Be,
        ReadFlBitMapA32BeRev,
        ReadFlBitMapA32Le,
        ReadFlBitMapA32LeRev,
        ReadFlBitMapA64Be,
        ReadFlBitMapA64BeRev,
        ReadFlBitMapA64Le,
        ReadFlBitMapA64LeRev,
        ReadFlBitMapA8,
        ReadFlBitMapA8Rev,
        ReadFlBitMapBe,
        ReadFlBitMapBeRev,
        ReadFlBitMapLe,
        ReadFlBitMapLeRev,
        ReadFlBoolA16Be,
        ReadFlBoolA16BeRev,
        ReadFlBoolA16Le,
        ReadFlBoolA16LeRev,
        ReadFlBoolA32Be,
        ReadFlBoolA32BeRev,
        ReadFlBoolA32Le,
        ReadFlBoolA32LeRev,
        ReadFlBoolA64Be,
        ReadFlBoolA64BeRev,
        ReadFlBoolA64Le,
        ReadFlBoolA64LeRev,
        ReadFlBoolA8,
        ReadFlBoolA8Rev,
        ReadFlBoolBe,
        ReadFlBoolBeRev,
        ReadFlBoolLe,
        ReadFlBoolLeRev,
        ReadFlFloat32Be,
        ReadFlFloat32BeRev,
        ReadFlFloat32Le,
        ReadFlFloat32LeRev,
        ReadFlFloat64Be,
        ReadFlFloat64BeRev,
        ReadFlFloat64Le,
        ReadFlFloat64LeRev,
        ReadFlFloatA32Be,
        ReadFlFloatA32BeRev,
        ReadFlFloatA32Le,
        ReadFlFloatA32LeRev,
        ReadFlFloatA64Be,
        ReadFlFloatA64BeRev,
        ReadFlFloatA64Le,
        ReadFlFloatA64LeRev,
        ReadFlSIntA16Be,
        ReadFlSIntA16BeRev,
        ReadFlSIntA16Le,
        ReadFlSIntA16LeRev,
        ReadFlSIntA32Be,
        ReadFlSIntA32BeRev,
        ReadFlSIntA32Le,
        ReadFlSIntA32LeRev,
        ReadFlSIntA64Be,
        ReadFlSIntA64BeRev,
        ReadFlSIntA64Le,
        ReadFlSIntA64LeRev,
        ReadFlSIntA8,
        ReadFlSIntA8Rev,
        ReadFlSIntBe,
        ReadFlSIntBeRev,
        ReadFlSIntLe,
        ReadFlSIntLeRev,
        ReadFlUIntA16Be,
        ReadFlUIntA16BeRev,
        ReadFlUIntA16Le,
        ReadFlUIntA16LeRev,
        ReadFlUIntA32Be,
        ReadFlUIntA32BeRev,
        ReadFlUIntA32Le,
        ReadFlUIntA32LeRev,
        ReadFlUIntA64Be,
        ReadFlUIntA64BeRev,
        ReadFlUIntA64Le,
        ReadFlUIntA64LeRev,
        ReadFlUIntA8,
        ReadFlUIntA8Rev,
        ReadFlUIntBe,
        ReadFlUIntBeRev,
        ReadFlUIntLe,
        ReadFlUIntLeRev,
        ReadNtStrUtf16,
        ReadNtStrUtf32,
        ReadNtStrUtf8,
        ReadVlSInt,
        ReadVlUInt,
        SaveVal,
        SetCurId,
        SetDsId,
        SetDsInfo,
        SetDst,
        SetErInfo,
        SetErt,
        SetPktContentLen,
        SetPktDiscErCounterSnap,
        SetPktEndDefClkVal,
        SetPktInfo,
        SetPktMagicNumber,
        SetPktSeqNum,
        SetPktTotalLen,
        UpdateDefClkVal,
        UpdateDefClkValFl
    )

public:
    using Sp = std::shared_ptr<Instr>;
    using FindInstrsCurrent = std::unordered_map<const Instr *, Index>;

protected:
    explicit Instr() noexcept = default;
    explicit Instr(Kind kind) noexcept;

public:
    virtual ~Instr() = default;
    virtual void accept(InstrVisitor& visitor) = 0;
    virtual void buildRawProcFromShared();

    // only used for debugging purposes
    std::string toStr(Size indent = 0) const;

    // only used to debug and for assertions
    bool isBeginReadData() const noexcept
    {
        switch (_theKind) {
        case Instr::Kind::BeginReadDlArray:
        case Instr::Kind::BeginReadDlBlob:
        case Instr::Kind::BeginReadDlStr:
        case Instr::Kind::BeginReadOptBoolSel:
        case Instr::Kind::BeginReadOptSIntSel:
        case Instr::Kind::BeginReadOptUIntSel:
        case Instr::Kind::BeginReadScope:
        case Instr::Kind::BeginReadSlArray:
        case Instr::Kind::BeginReadSlBlob:
        case Instr::Kind::BeginReadSlStr:
        case Instr::Kind::BeginReadSlUuidArray:
        case Instr::Kind::BeginReadSlUuidBlob:
        case Instr::Kind::BeginReadStruct:
        case Instr::Kind::BeginReadVarSIntSel:
        case Instr::Kind::BeginReadVarUIntSel:
        case Instr::Kind::ReadFlBitArrayA16Be:
        case Instr::Kind::ReadFlBitArrayA16BeRev:
        case Instr::Kind::ReadFlBitArrayA16Le:
        case Instr::Kind::ReadFlBitArrayA16LeRev:
        case Instr::Kind::ReadFlBitArrayA32Be:
        case Instr::Kind::ReadFlBitArrayA32BeRev:
        case Instr::Kind::ReadFlBitArrayA32Le:
        case Instr::Kind::ReadFlBitArrayA32LeRev:
        case Instr::Kind::ReadFlBitArrayA64Be:
        case Instr::Kind::ReadFlBitArrayA64BeRev:
        case Instr::Kind::ReadFlBitArrayA64Le:
        case Instr::Kind::ReadFlBitArrayA64LeRev:
        case Instr::Kind::ReadFlBitArrayA8:
        case Instr::Kind::ReadFlBitArrayA8Rev:
        case Instr::Kind::ReadFlBitArrayBe:
        case Instr::Kind::ReadFlBitArrayBeRev:
        case Instr::Kind::ReadFlBitArrayLe:
        case Instr::Kind::ReadFlBitArrayLeRev:
        case Instr::Kind::ReadFlBitMapA16Be:
        case Instr::Kind::ReadFlBitMapA16BeRev:
        case Instr::Kind::ReadFlBitMapA16Le:
        case Instr::Kind::ReadFlBitMapA16LeRev:
        case Instr::Kind::ReadFlBitMapA32Be:
        case Instr::Kind::ReadFlBitMapA32BeRev:
        case Instr::Kind::ReadFlBitMapA32Le:
        case Instr::Kind::ReadFlBitMapA32LeRev:
        case Instr::Kind::ReadFlBitMapA64Be:
        case Instr::Kind::ReadFlBitMapA64BeRev:
        case Instr::Kind::ReadFlBitMapA64Le:
        case Instr::Kind::ReadFlBitMapA64LeRev:
        case Instr::Kind::ReadFlBitMapA8:
        case Instr::Kind::ReadFlBitMapA8Rev:
        case Instr::Kind::ReadFlBitMapBe:
        case Instr::Kind::ReadFlBitMapBeRev:
        case Instr::Kind::ReadFlBitMapLe:
        case Instr::Kind::ReadFlBitMapLeRev:
        case Instr::Kind::ReadFlBoolA16Be:
        case Instr::Kind::ReadFlBoolA16BeRev:
        case Instr::Kind::ReadFlBoolA16Le:
        case Instr::Kind::ReadFlBoolA16LeRev:
        case Instr::Kind::ReadFlBoolA32Be:
        case Instr::Kind::ReadFlBoolA32BeRev:
        case Instr::Kind::ReadFlBoolA32Le:
        case Instr::Kind::ReadFlBoolA32LeRev:
        case Instr::Kind::ReadFlBoolA64Be:
        case Instr::Kind::ReadFlBoolA64BeRev:
        case Instr::Kind::ReadFlBoolA64Le:
        case Instr::Kind::ReadFlBoolA64LeRev:
        case Instr::Kind::ReadFlBoolA8:
        case Instr::Kind::ReadFlBoolA8Rev:
        case Instr::Kind::ReadFlBoolBe:
        case Instr::Kind::ReadFlBoolBeRev:
        case Instr::Kind::ReadFlBoolLe:
        case Instr::Kind::ReadFlBoolLeRev:
        case Instr::Kind::ReadFlFloat32Be:
        case Instr::Kind::ReadFlFloat32BeRev:
        case Instr::Kind::ReadFlFloat32Le:
        case Instr::Kind::ReadFlFloat32LeRev:
        case Instr::Kind::ReadFlFloat64Be:
        case Instr::Kind::ReadFlFloat64BeRev:
        case Instr::Kind::ReadFlFloat64Le:
        case Instr::Kind::ReadFlFloat64LeRev:
        case Instr::Kind::ReadFlFloatA32Be:
        case Instr::Kind::ReadFlFloatA32BeRev:
        case Instr::Kind::ReadFlFloatA32Le:
        case Instr::Kind::ReadFlFloatA32LeRev:
        case Instr::Kind::ReadFlFloatA64Be:
        case Instr::Kind::ReadFlFloatA64BeRev:
        case Instr::Kind::ReadFlFloatA64Le:
        case Instr::Kind::ReadFlFloatA64LeRev:
        case Instr::Kind::ReadFlSIntA16Be:
        case Instr::Kind::ReadFlSIntA16BeRev:
        case Instr::Kind::ReadFlSIntA16Le:
        case Instr::Kind::ReadFlSIntA16LeRev:
        case Instr::Kind::ReadFlSIntA32Be:
        case Instr::Kind::ReadFlSIntA32BeRev:
        case Instr::Kind::ReadFlSIntA32Le:
        case Instr::Kind::ReadFlSIntA32LeRev:
        case Instr::Kind::ReadFlSIntA64Be:
        case Instr::Kind::ReadFlSIntA64BeRev:
        case Instr::Kind::ReadFlSIntA64Le:
        case Instr::Kind::ReadFlSIntA64LeRev:
        case Instr::Kind::ReadFlSIntA8:
        case Instr::Kind::ReadFlSIntA8Rev:
        case Instr::Kind::ReadFlSIntBe:
        case Instr::Kind::ReadFlSIntBeRev:
        case Instr::Kind::ReadFlSIntLe:
        case Instr::Kind::ReadFlSIntLeRev:
        case Instr::Kind::ReadFlUIntA16Be:
        case Instr::Kind::ReadFlUIntA16BeRev:
        case Instr::Kind::ReadFlUIntA16Le:
        case Instr::Kind::ReadFlUIntA16LeRev:
        case Instr::Kind::ReadFlUIntA32Be:
        case Instr::Kind::ReadFlUIntA32BeRev:
        case Instr::Kind::ReadFlUIntA32Le:
        case Instr::Kind::ReadFlUIntA32LeRev:
        case Instr::Kind::ReadFlUIntA64Be:
        case Instr::Kind::ReadFlUIntA64BeRev:
        case Instr::Kind::ReadFlUIntA64Le:
        case Instr::Kind::ReadFlUIntA64LeRev:
        case Instr::Kind::ReadFlUIntA8:
        case Instr::Kind::ReadFlUIntA8Rev:
        case Instr::Kind::ReadFlUIntBe:
        case Instr::Kind::ReadFlUIntBeRev:
        case Instr::Kind::ReadFlUIntLe:
        case Instr::Kind::ReadFlUIntLeRev:
        case Instr::Kind::ReadNtStrUtf16:
        case Instr::Kind::ReadNtStrUtf32:
        case Instr::Kind::ReadNtStrUtf8:
        case Instr::Kind::ReadVlSInt:
        case Instr::Kind::ReadVlUInt:
            return true;

        default:
            return false;
        }
    }

    // only used to debug and for assertions
    bool isEndReadData() const noexcept
    {
        switch (_theKind) {
        case Instr::Kind::EndReadSlArray:
        case Instr::Kind::EndReadDlArray:
        case Instr::Kind::EndReadSlStr:
        case Instr::Kind::EndReadDlStr:
        case Instr::Kind::EndReadSlBlob:
        case Instr::Kind::EndReadDlBlob:
        case Instr::Kind::EndReadStruct:
        case Instr::Kind::EndReadVarSIntSel:
        case Instr::Kind::EndReadVarUIntSel:
        case Instr::Kind::EndReadOptBoolSel:
        case Instr::Kind::EndReadOptSIntSel:
        case Instr::Kind::EndReadOptUIntSel:
            return true;

        default:
            return false;
        }
    }

    Kind kind() const noexcept
    {
        assert(_theKind != Kind::Unset);
        return _theKind;
    }

private:
    virtual std::string _toStr(Size indent = 0) const;

private:
    const Kind _theKind = Kind::Unset;
};

/*
 * "Read data" procedure instruction abstract class.
 */
class ReadDataInstr :
    public Instr
{
protected:
    explicit ReadDataInstr(Kind kind, const StructureMemberType *memberType, const DataType& dt);

public:
    /*
     * `memberType` can be `nullptr` if this is the scope's root read
     * instruction.
     */
    explicit ReadDataInstr(const StructureMemberType *memberType, const DataType& dt);

    const DataType& dt() const noexcept
    {
        return *_dt;
    }

    const StructureMemberType *memberType() const noexcept
    {
        return _memberType;
    }

    unsigned int align() const noexcept
    {
        return _align;
    }

protected:
    std::string _commonToStr() const;

private:
    const StructureMemberType * const _memberType;
    const DataType * const _dt;
    const unsigned int _align;
};

/*
 * "Save value" procedure instruction.
 *
 * This instruction requires the VM to save the last decoded integer
 * value to a position (index) in its saved value vector so that it can
 * be used later (for the length of a dynamic-length array/string/BLOB
 * or for the selector of a variant/optional).
 */
class SaveValInstr final :
    public Instr
{
public:
    explicit SaveValInstr(Index pos);

    Index pos() const noexcept
    {
        return _pos;
    }

    void pos(const Index pos) noexcept
    {
        _pos = pos;
    }

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    Index _pos;
};

/*
 * "Set packet end clock value" procedure instruction.
 *
 * This instruction indicates to the VM that the last decoded integer
 * value is the packet end clock value.
 */
class SetPktEndDefClkValInstr final :
    public Instr
{
public:
    explicit SetPktEndDefClkValInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Read fixed-length bit array" procedure instruction.
 */
class ReadFlBitArrayInstr :
    public ReadDataInstr
{
protected:
    explicit ReadFlBitArrayInstr(Kind kind, const StructureMemberType *memberType,
                                 const DataType& dt);

public:
    explicit ReadFlBitArrayInstr(const StructureMemberType *memberType, const DataType& dt);

    unsigned int len() const noexcept
    {
        return _len;
    }

    ByteOrder bo() const noexcept
    {
        return _bo;
    }

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthBitArrayType& flBitArrayType() const noexcept
    {
        return static_cast<const FixedLengthBitArrayType&>(this->dt());
    }

protected:
    std::string _commonToStr() const;

private:
    std::string _toStr(const Size indent = 0) const override;

private:
    const unsigned int _len;
    const ByteOrder _bo;
};

/*
 * "Read fixed-length bit map" procedure instruction.
 */
class ReadFlBitMapInstr :
    public ReadFlBitArrayInstr
{
public:
    explicit ReadFlBitMapInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthBitMapType& bitMapType() const noexcept
    {
        return static_cast<const FixedLengthBitMapType&>(this->dt());
    }
};

/*
 * "Read fixed-length boolean" procedure instruction.
 */
class ReadFlBoolInstr :
    public ReadFlBitArrayInstr
{
public:
    explicit ReadFlBoolInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthBooleanType& boolType() const noexcept
    {
        return static_cast<const FixedLengthBooleanType&>(this->dt());
    }
};

/*
 * "Read fixed-length integer" procedure instruction.
 */
class ReadFlIntInstr :
    public ReadFlBitArrayInstr
{
protected:
    explicit ReadFlIntInstr(Kind kind, const StructureMemberType *memberType, const DataType& dt);

public:
    explicit ReadFlIntInstr(const StructureMemberType *memberType, const DataType& dt);
};

/*
 * "Read fixed-length signed integer" procedure instruction.
 */
class ReadFlSIntInstr :
    public ReadFlIntInstr
{
protected:
    explicit ReadFlSIntInstr(Kind kind, const StructureMemberType *memberType, const DataType& dt);

public:
    explicit ReadFlSIntInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthSignedIntegerType& sIntType() const noexcept
    {
        return static_cast<const FixedLengthSignedIntegerType&>(this->dt());
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Read fixed-length unsigned integer" procedure instruction.
 */
class ReadFlUIntInstr :
    public ReadFlIntInstr
{
protected:
    explicit ReadFlUIntInstr(Kind kind, const StructureMemberType *memberType, const DataType& dt);

public:
    explicit ReadFlUIntInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthUnsignedIntegerType& uIntType() const noexcept
    {
        return static_cast<const FixedLengthUnsignedIntegerType&>(this->dt());
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Read fixed-length floating point number" procedure instruction.
 */
class ReadFlFloatInstr final :
    public ReadFlBitArrayInstr
{
public:
    explicit ReadFlFloatInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const FixedLengthFloatingPointNumberType& floatType() const noexcept
    {
        return static_cast<const FixedLengthFloatingPointNumberType&>(this->dt());
    }
};

/*
 * "Read variable-length integer" procedure instruction.
 */
class ReadVlIntInstr :
    public ReadDataInstr
{
public:
    explicit ReadVlIntInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Read null-terminated string" procedure instruction.
 */
class ReadNtStrInstr final :
    public ReadDataInstr
{
public:
    explicit ReadNtStrInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const NullTerminatedStringType& strType() const noexcept
    {
        return static_cast<const NullTerminatedStringType&>(this->dt());
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Begin reading compound data" procedure instruction abstract class.
 *
 * This instruction contains a subprocedure to execute.
 */
class BeginReadCompoundInstr :
    public ReadDataInstr
{
protected:
    explicit BeginReadCompoundInstr(Kind kind, const StructureMemberType *memberType,
                                    const DataType& dt);

public:
    const Proc& proc() const noexcept
    {
        return _proc;
    }

    Proc& proc() noexcept
    {
        return _proc;
    }

    void buildRawProcFromShared() override;

protected:
    std::string _procToStr(const Size indent) const
    {
        return _proc.toStr(indent);
    }

private:
    Proc _proc;
};

/*
 * "End reading data" procedure instruction.
 *
 * If the kind of this instruction is `EndReadStruct`, then the VM must
 * stop executing the current procedure and continue executing the
 * parent procedure.
 *
 * For all instruction kinds, this instruction requires the VM to set an
 * `EndElement` as the current element.
 */
class EndReadDataInstr :
    public ReadDataInstr
{
public:
    explicit EndReadDataInstr(Kind kind, const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Begin reading structure" procedure instruction.
 */
class BeginReadStructInstr final :
    public BeginReadCompoundInstr
{
public:
    explicit BeginReadStructInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const StructureType& structType() const noexcept
    {
        return static_cast<const StructureType&>(this->dt());
    }

private:
    std::string _toStr(Size indent = 0) const override;
};

/*
 * "Begin reading scope" procedure instruction.
 *
 * This is the top-level instruction to start reading a whole scope
 * (packet header, packet context, event record payload, etc.).
 */
class BeginReadScopeInstr final :
    public Instr
{
public:
    explicit BeginReadScopeInstr(Scope scope, unsigned int align);
    void buildRawProcFromShared() override;

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    Scope scope() const noexcept
    {
        return _scope;
    }

    const Proc& proc() const noexcept
    {
        return _proc;
    }

    Proc& proc() noexcept
    {
        return _proc;
    }

    unsigned int align() const noexcept
    {
        return _align;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Scope _scope;
    const unsigned int _align = 1;
    Proc _proc;
};

/*
 * "End reading scope" procedure instruction.
 *
 * This requires the VM to stop executing the current procedure and
 * continue executing the parent procedure.
 */
class EndReadScopeInstr final :
    public Instr
{
public:
    explicit EndReadScopeInstr(Scope scope);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    Scope scope() const noexcept
    {
        return _scope;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Scope _scope;
};

/*
 * "Begin reading static-length array" procedure instruction.
 *
 * The VM must execute the subprocedure `len()` times.
 */
class BeginReadSlArrayInstr :
    public BeginReadCompoundInstr
{
protected:
    explicit BeginReadSlArrayInstr(Kind kind, const StructureMemberType *memberType,
                                   const DataType& dt);

public:
    explicit BeginReadSlArrayInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const StaticLengthArrayType& slArrayType() const noexcept
    {
        return static_cast<const StaticLengthArrayType&>(this->dt());
    }

    Size len() const noexcept
    {
        return _len;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Size _len;
};

/*
 * "Begin reading static-length string" procedure instruction.
 *
 * maxLen() indicates the maximum length (bytes) of the static-length
 * string to read.
 */
class BeginReadSlStrInstr final :
    public ReadDataInstr
{
public:
    explicit BeginReadSlStrInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const StaticLengthStringType& slStrType() const noexcept
    {
        return static_cast<const StaticLengthStringType&>(this->dt());
    }

    Size maxLen() const noexcept
    {
        return _maxLen;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Size _maxLen;
};

/*
 * "Begin reading static-length UUID array" procedure instruction.
 *
 * This is a specialized instruction to read the 16 metadata stream UUID
 * bytes of a packet header to emit `MetadataStreamUuidElement`.
 */
class BeginReadSlUuidArrayInstr final :
    public BeginReadSlArrayInstr
{
public:
    explicit BeginReadSlUuidArrayInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Begin reading dynamic-length array" procedure instruction.
 *
 * The VM must use lenPos() to retrieve the saved value which contains
 * the length of the dynamic-length array, and then execute the
 * subprocedure this number of times.
 */
class BeginReadDlArrayInstr final :
    public BeginReadCompoundInstr
{
public:
    explicit BeginReadDlArrayInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const DynamicLengthArrayType& dlArrayType() const noexcept
    {
        return static_cast<const DynamicLengthArrayType&>(this->dt());
    }

    Index lenPos() const noexcept
    {
        return _lenPos;
    }

    void lenPos(const Index lenPos) noexcept
    {
        _lenPos = lenPos;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    Index _lenPos = UINT64_C(-1);
};

/*
 * "Begin reading dynamic-length string" procedure instruction.
 *
 * The VM must use maxLenPos() to retrieve the saved value which
 * contains the maximum length (bytes) of the dynamic-length string.
 */
class BeginReadDlStrInstr final :
    public ReadDataInstr
{
public:
    explicit BeginReadDlStrInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const DynamicLengthStringType& dlStrType() const noexcept
    {
        return static_cast<const DynamicLengthStringType&>(this->dt());
    }

    Index maxLenPos() const noexcept
    {
        return _maxLenPos;
    }

    void maxLenPos(const Index maxLenPos) noexcept
    {
        _maxLenPos = maxLenPos;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    Index _maxLenPos = UINT64_C(-1);
};

/*
 * "Begin reading static-length BLOB" procedure instruction.
 *
 * len() indicates the length (bytes) of the static-length BLOB to read.
 */
class BeginReadSlBlobInstr :
    public ReadDataInstr
{
protected:
    explicit BeginReadSlBlobInstr(Kind kind, const StructureMemberType *memberType,
                                  const DataType& dt);

public:
    explicit BeginReadSlBlobInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const StaticLengthBlobType& slBlobType() const noexcept
    {
        return static_cast<const StaticLengthBlobType&>(this->dt());
    }

    Size len() const noexcept
    {
        return _len;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Size _len;
};

/*
 * "Begin reading static-length UUID BLOB" procedure instruction.
 *
 * This is a specialized instruction to read the 16 UUID bytes of a
 * packet header to emit `MetadataStreamUuidElement`.
 */
class BeginReadSlUuidBlobInstr final :
    public BeginReadSlBlobInstr
{
public:
    explicit BeginReadSlUuidBlobInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Begin reading dynamic-length BLOB" procedure instruction.
 *
 * The VM must use lenPos() to retrieve the saved value which contains
 * the length (bytes) of the dynamic-length BLOB.
 */
class BeginReadDlBlobInstr final :
    public ReadDataInstr
{
public:
    explicit BeginReadDlBlobInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

    const DynamicLengthBlobType& dlBlobType() const noexcept
    {
        return static_cast<const DynamicLengthBlobType&>(this->dt());
    }

    Index lenPos() const noexcept
    {
        return _lenPos;
    }

    void lenPos(const Index lenPos) noexcept
    {
        _lenPos = lenPos;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    Index _lenPos = UINT64_C(-1);
};

/*
 * Option of a "read variant" procedure instruction.
 */
template <typename VarTypeOptT>
class ReadVarInstrOpt final
{
public:
    using Opt = VarTypeOptT;
    using SelRangeSet = typename Opt::SelectorRangeSet;
    using Val = typename SelRangeSet::Value;

public:
    explicit ReadVarInstrOpt() = default;
    ReadVarInstrOpt(const ReadVarInstrOpt<VarTypeOptT>& opt) = default;
    ReadVarInstrOpt(ReadVarInstrOpt<VarTypeOptT>&& opt) = default;
    ReadVarInstrOpt<VarTypeOptT>& operator=(const ReadVarInstrOpt<VarTypeOptT>& opt) = default;
    ReadVarInstrOpt<VarTypeOptT>& operator=(ReadVarInstrOpt<VarTypeOptT>&& opt) = default;

    explicit ReadVarInstrOpt(const VarTypeOptT& opt) :
        _opt {&opt}
    {
    }

    void buildRawProcFromShared()
    {
        _proc.buildRawProcFromShared();
    }

    bool contains(const Val val) const noexcept
    {
        return _opt->selectorRanges().contains(val);
    }

    const Opt& opt() const noexcept
    {
        return *_opt;
    }

    const SelRangeSet& selRanges() const noexcept
    {
        return _opt->selectorRanges();
    }

    const Proc& proc() const noexcept
    {
        return _proc;
    }

    Proc& proc() noexcept
    {
        return _proc;
    }

    std::string toStr(const Size indent = 0) const
    {
        std::ostringstream ss;

        ss << internal::indent(indent) << "<var opt>";

        for (const auto& range : _opt->selectorRanges()) {
            ss << " [" << range.lower() << ", " << range.upper() << "]";
        }

        ss << std::endl << _proc.toStr(indent + 1);
        return ss.str();
    }

private:
    const Opt *_opt;

    /*
     * Contained pointers are not owned by this object: they are owned
     * by the variant instruction object which contains the options.
     */
    Proc _proc;
};

inline std::string _strProp(const std::string& prop)
{
    std::string rProp;

    rProp = "\033[1m";
    rProp += prop;
    rProp += "\033[0m=";
    return rProp;
}

/*
 * "Begin reading variant" procedure instruction template.
 *
 * The VM must use selPos() to retrieve the saved value which is the
 * selector of the variant, find the corresponding option for this
 * selector value, and then execute the subprocedure of the option.
 */
template <typename VarTypeT, Instr::Kind SelfKindV>
class BeginReadVarInstr :
    public ReadDataInstr
{
public:
    using Opt = ReadVarInstrOpt<typename VarTypeT::Option>;
    using Opts = std::vector<Opt>;

protected:
    explicit BeginReadVarInstr(const StructureMemberType * const memberType, const DataType& dt) :
        ReadDataInstr {SelfKindV, memberType, dt}
    {
        auto& varType = static_cast<const VarTypeT&>(dt);

        for (auto& opt : varType.options()) {
            _opts.emplace_back(*opt);
        }
    }

public:
    void buildRawProcFromShared() override
    {
        for (auto& opt : _opts) {
            opt.buildRawProcFromShared();
        }
    }

    const VarTypeT& varType() const noexcept
    {
        return static_cast<const VarTypeT&>(this->dt());
    }

    const Opts& opts() const noexcept
    {
        return _opts;
    }

    Opts& opts() noexcept
    {
        return _opts;
    }

    const Proc *procForSelVal(const typename Opt::Val selVal) const noexcept
    {
        for (auto& opt : _opts) {
            if (opt.contains(selVal)) {
                return &opt.proc();
            }
        }

        return nullptr;
    }

    Index selPos() const noexcept
    {
        return _selPos;
    }

    void selPos(const Index pos) noexcept
    {
        _selPos = pos;
    }

private:
    std::string _toStr(const Size indent = 0) const override
    {
        std::ostringstream ss;

        ss << this->_commonToStr() << " " << _strProp("sel-pos") << _selPos << std::endl;

        for (const auto& opt : _opts) {
            ss << opt.toStr(indent + 1);
        }

        return ss.str();
    }

private:
    Opts _opts;
    Index _selPos;
};

class BeginReadVarUIntSelInstr final :
    public BeginReadVarInstr<VariantWithUnsignedIntegerSelectorType,
                             Instr::Kind::BeginReadVarUIntSel>
{
public:
    explicit BeginReadVarUIntSelInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

class BeginReadVarSIntSelInstr final :
    public BeginReadVarInstr<VariantWithSignedIntegerSelectorType,
                             Instr::Kind::BeginReadVarSIntSel>
{
public:
    explicit BeginReadVarSIntSelInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Begin reading optional" abstract procedure instruction class.
 *
 * The VM must use selPos() to retrieve the saved value which is the
 * selector of the optional, and, depending on the value and the type of
 * optional, execute its subprocedure.
 */
class BeginReadOptInstr :
    public BeginReadCompoundInstr
{
protected:
    explicit BeginReadOptInstr(Kind kind, const StructureMemberType *memberType,
                               const DataType& dt);

public:
    const OptionalType& optType() const noexcept
    {
        return static_cast<const OptionalType&>(this->dt());
    }

    Index selPos() const noexcept
    {
        return _selPos;
    }

    void selPos(const Index pos) noexcept
    {
        _selPos = pos;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    Index _selPos = UINT64_C(-1);
};

/*
 * "Begin reading optional with boolean selector" procedure instruction.
 */
class BeginReadOptBoolSelInstr :
    public BeginReadOptInstr
{
public:
    explicit BeginReadOptBoolSelInstr(const StructureMemberType *memberType, const DataType& dt);

    const OptionalWithBooleanSelectorType& optType() const noexcept
    {
        return static_cast<const OptionalWithBooleanSelectorType&>(this->dt());
    }

    bool isEnabled(const bool selVal) const noexcept
    {
        return selVal;
    }

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Begin reading optional with integer selector" procedure instruction
 * template.
 */
template <typename OptTypeT, Instr::Kind SelfKindV>
class BeginReadOptIntSelInstr :
    public BeginReadOptInstr
{
public:
    using SelRanges = typename OptTypeT::SelectorRangeSet;

protected:
    explicit BeginReadOptIntSelInstr(const StructureMemberType * const memberType,
                                     const DataType& dt) :
        BeginReadOptInstr {SelfKindV, memberType, dt},
        _selRanges {static_cast<const OptTypeT&>(dt).selectorRanges()}
    {
    }

public:
    const OptTypeT& optType() const noexcept
    {
        return static_cast<const OptTypeT&>(this->dt());
    }

    const SelRanges& selRanges() const noexcept
    {
        return _selRanges;
    }

    bool isEnabled(const typename SelRanges::Value selVal) const noexcept
    {
        return _selRanges.contains(selVal);
    }

private:
    SelRanges _selRanges;
};

/*
 * "Begin reading optional with unsigned integer selector" procedure
 * instruction.
 */
class BeginReadOptUIntSelInstr :
    public BeginReadOptIntSelInstr<OptionalWithUnsignedIntegerSelectorType,
                                   Instr::Kind::BeginReadOptUIntSel>
{
public:
    explicit BeginReadOptUIntSelInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Begin reading optional with signed integer selector" procedure
 * instruction.
 */
class BeginReadOptSIntSelInstr :
    public BeginReadOptIntSelInstr<OptionalWithSignedIntegerSelectorType,
                                   Instr::Kind::BeginReadOptSIntSel>
{
public:
    explicit BeginReadOptSIntSelInstr(const StructureMemberType *memberType, const DataType& dt);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set current ID" procedure instruction.
 *
 * This instruction requires the VM to set the current ID to the last
 * decoded value. This is either the current data stream type ID or the
 * current event record type ID.
 */
class SetCurIdInstr final :
    public Instr
{
public:
    explicit SetCurIdInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set current type" procedure instruction abstract class.
 *
 * This instruction asks the VM to set the current data stream or event
 * record type using the current ID, or using `fixedId()` if it exists.
 *
 * TODO: This ☝ doesn't seem like the correct approach.
 */
class SetTypeInstr :
    public Instr
{
protected:
    explicit SetTypeInstr(Kind kind, boost::optional<TypeId> fixedId);

public:
    const boost::optional<TypeId>& fixedId() const noexcept
    {
        return _fixedId;
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const boost::optional<TypeId> _fixedId;
};

/*
 * "Set current data stream type" procedure instruction.
 */
class SetDstInstr final :
    public SetTypeInstr
{
public:
    explicit SetDstInstr(boost::optional<TypeId> fixedId = boost::none);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set current event record type" procedure instruction.
 */
class SetErtInstr final :
    public SetTypeInstr
{
public:
    explicit SetErtInstr(boost::optional<TypeId> fixedId = boost::none);

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set packet sequence number" procedure instruction.
 *
 * This instruction requires the VM to set the packet sequence number to
 * the last decoded value.
 */
class SetPktSeqNumInstr final :
    public Instr
{
public:
    explicit SetPktSeqNumInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set packet discarded event record counter snapshot" procedure
 * instruction.
 *
 * This instruction requires the VM to set the packet discarded event
 * record counter snapshot to the last decoded value.
 */
class SetPktDiscErCounterSnapInstr final :
    public Instr
{
public:
    explicit SetPktDiscErCounterSnapInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set data stream ID" procedure instruction.
 *
 * This instruction requires the VM to set the data stream ID to the
 * last decoded value.
 *
 * This is NOT the current data stream _type_ ID. It's sometimes called
 * the "data stream instance ID".
 */
class SetDsIdInstr final :
    public Instr
{
public:
    explicit SetDsIdInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set data stream info" procedure instruction.
 *
 * This instruction requires the VM to set and emit the data stream
 * info element.
 */
class SetDsInfoInstr final :
    public Instr
{
public:
    explicit SetDsInfoInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set packet info" procedure instruction.
 *
 * This instruction requires the VM to set and emit the packet info
 * element.
 */
class SetPktInfoInstr final :
    public Instr
{
public:
    explicit SetPktInfoInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set event record info" procedure instruction.
 *
 * This instruction requires the VM to set and emit the event record
 * info element.
 */
class SetErInfoInstr final :
    public Instr
{
public:
    explicit SetErInfoInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set expected packet total length" procedure instruction.
 *
 * This instruction requires the VM to set the expected packet total
 * length (bits) to the last decoded value.
 */
class SetExpectedPktTotalLenInstr final :
    public Instr
{
public:
    explicit SetExpectedPktTotalLenInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Set expected packet content length" procedure instruction.
 *
 * This instruction requires the VM to set the expected packet content
 * length (bits) to the last decoded value.
 */
class SetExpectedPktContentLenInstr final :
    public Instr
{
public:
    explicit SetExpectedPktContentLenInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Update clock value" procedure instruction.
 *
 * This instruction requires the VM to update the value of the default
 * clock from the last decoded unsigned integer value.
 */
class UpdateDefClkValInstr :
    public Instr
{
protected:
    explicit UpdateDefClkValInstr(Instr::Kind kind);

public:
    explicit UpdateDefClkValInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Update clock value from fixed-length unsigned integer" procedure
 * instruction.
 *
 * This instruction requires the VM to update the value of the default
 * clock from the last decoded fixed-length unsigned integer value.
 */
class UpdateDefClkValFlInstr final :
    public UpdateDefClkValInstr
{
public:
    explicit UpdateDefClkValFlInstr(Size len);

    Size len() const noexcept
    {
        return _len;
    }

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }

private:
    std::string _toStr(Size indent = 0) const override;

private:
    const Size _len;
};

/*
 * "Set packet magic number" procedure instruction.
 *
 * This instruction requires the VM to use the last decoded value as the
 * packet magic number.
 */
class SetPktMagicNumberInstr final :
    public Instr
{
public:
    explicit SetPktMagicNumberInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "End packet preamble procedure" procedure instruction.
 *
 * This instruction indicates that the packet preamble procedure
 * containing it has no more instructions.
 */
class EndPktPreambleProcInstr final :
    public Instr
{
public:
    explicit EndPktPreambleProcInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "End data stream packet preamble procedure" procedure instruction.
 *
 * This instruction indicates that the data stream packet preamble
 * procedure containing it has no more instructions.
 */
class EndDsPktPreambleProcInstr final :
    public Instr
{
public:
    explicit EndDsPktPreambleProcInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "End data stream event record preamble procedure" procedure
 * instruction.
 *
 * This instruction indicates that the data stream event record preamble
 * procedure containing it has no more instructions.
 */
class EndDsErPreambleProcInstr final :
    public Instr
{
public:
    explicit EndDsErPreambleProcInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "End event record type procedure" procedure instruction.
 *
 * This instruction indicates that the event record type procedure
 * containing it has no more instructions.
 */
class EndErProcInstr final :
    public Instr
{
public:
    explicit EndErProcInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * "Decrement remaining elements" procedure instruction.
 *
 * When reading an array, this instruction requires the VM to decrement
 * the number of remaining elements to read.
 *
 * It's placed just before an "end read compound data" instruction as a
 * trade-off between checking if we're in an array every time we end a
 * compound data, or having this decrementation instruction even for
 * simple arrays of scalar elements.
 */
class DecrRemainingElemsInstr final :
    public Instr
{
public:
    explicit DecrRemainingElemsInstr();

    void accept(InstrVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

/*
 * Event record procedure.
 */
class ErProc final
{
public:
    explicit ErProc(const EventRecordType& ert);
    std::string toStr(Size indent) const;
    void buildRawProcFromShared();

    Proc& proc() noexcept
    {
        return _proc;
    }

    const Proc& proc() const noexcept
    {
        return _proc;
    }

    const EventRecordType& ert() const noexcept
    {
        return *_ert;
    }

private:
    const EventRecordType * const _ert;
    Proc _proc;
};

/*
 * Packet procedure for any data stream of a given type.
 */
class DsPktProc final
{
public:
    using ErProcsMap = std::unordered_map<TypeId, std::unique_ptr<ErProc>>;
    using ErProcsVec = std::vector<std::unique_ptr<ErProc>>;
    using ForEachErProcFunc = std::function<void (ErProc&)>;

public:
    explicit DsPktProc(const DataStreamType& dst);
    const ErProc *operator[](TypeId id) const noexcept;
    const ErProc *singleErProc() const noexcept;
    void addErProc(std::unique_ptr<ErProc> erProc);
    std::string toStr(Size indent) const;
    void buildRawProcFromShared();
    void setErAlign();

    template <typename FuncT>
    void forEachErProc(FuncT&& func)
    {
        for (auto& erProc : _erProcsVec) {
            if (erProc) {
                func(*erProc);
            }
        }

        for (auto& idErProcUpPair : _erProcsMap) {
            func(*idErProcUpPair.second);
        }
    }

    Proc& pktPreambleProc() noexcept
    {
        return _pktPreambleProc;
    }

    const Proc& pktPreambleProc() const noexcept
    {
        return _pktPreambleProc;
    }

    Proc& erPreambleProc() noexcept
    {
        return _erPreambleProc;
    }

    const Proc& erPreambleProc() const noexcept
    {
        return _erPreambleProc;
    }

    ErProcsMap& erProcsMap() noexcept
    {
        return _erProcsMap;
    }

    ErProcsVec& erProcsVec() noexcept
    {
        return _erProcsVec;
    }

    Size erProcsCount() const noexcept
    {
        return _erProcsMap.size() + _erProcsVec.size();
    }

    const DataStreamType& dst() const noexcept
    {
        return *_dst;
    }

    unsigned int erAlign() const noexcept
    {
        return _erAlign;
    }

private:
    const DataStreamType * const _dst;
    Proc _pktPreambleProc;
    Proc _erPreambleProc;
    unsigned int _erAlign = 1;

    /*
     * We have both a vector and a map here to store event record
     * procedures. Typically, event record type IDs are contiguous
     * within a given trace; storing them in the vector makes a more
     * efficient lookup afterwards if this is possible. For outliers, we
     * use the (slower) map.
     *
     * _erProcsVec can contain both event record procedures and null
     * pointers. _erProcsMap contains only event record procedures.
     */
    ErProcsVec _erProcsVec;
    ErProcsMap _erProcsMap;
};

/*
 * Packet procedure.
 *
 * Such an object is owned by a `TraceType` object, and it's not public.
 * This means that all the pointers to anything inside the owning
 * `TraceType` object are always safe to use.
 *
 * Any object which needs to access a `PktProc` object must own its
 * owning `TraceType` object. For example (ownership tree):
 *
 *     User
 *       Element sequence iterator
 *         VM
 *           Trace type
 *             Packet procedure
 */
class PktProc final
{
public:
    using DsPktProcs = std::unordered_map<TypeId, std::unique_ptr<DsPktProc>>;

public:
    explicit PktProc(const TraceType &traceType);
    const DsPktProc *operator[](TypeId id) const noexcept;
    const DsPktProc *singleDsPktProc() const noexcept;
    std::string toStr(Size indent) const;
    void buildRawProcFromShared();

    const TraceType& traceType() const noexcept
    {
        return *_traceType;
    }

    DsPktProcs& dsPktProcs() noexcept
    {
        return _dsPktProcs;
    }

    Size dsPktProcsCount() const noexcept
    {
        return _dsPktProcs.size();
    }

    Proc& preambleProc() noexcept
    {
        return _preambleProc;
    }

    const Proc& preambleProc() const noexcept
    {
        return _preambleProc;
    }

    Size savedValsCount() const noexcept
    {
        return _savedValsCount;
    }

    void savedValsCount(const Size savedValsCount)
    {
        _savedValsCount = savedValsCount;
    }

private:
    const TraceType * const _traceType;
    DsPktProcs _dsPktProcs;
    Size _savedValsCount = 0;
    Proc _preambleProc;
};

inline ReadDataInstr& instrAsReadData(Instr& instr) noexcept
{
    return static_cast<ReadDataInstr&>(instr);
}

inline BeginReadScopeInstr& instrAsBeginReadScope(Instr& instr) noexcept
{
    return static_cast<BeginReadScopeInstr&>(instr);
}

inline BeginReadStructInstr& instrAsBeginReadStruct(Instr& instr) noexcept
{
    return static_cast<BeginReadStructInstr&>(instr);
}

} // namespace internal
} // namespace yactfr

#endif // YACTFR_INTERNAL_PROC_HPP
