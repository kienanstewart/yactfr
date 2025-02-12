/*
 * Copyright (C) 2015-2024 Philippe Proulx <eepp.ca>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef YACTFR_INTERNAL_METADATA_PSEUDO_TYPES_HPP
#define YACTFR_INTERNAL_METADATA_PSEUDO_TYPES_HPP

#include <memory>
#include <vector>
#include <cstring>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <yactfr/aliases.hpp>
#include <yactfr/metadata/aliases.hpp>
#include <yactfr/metadata/data-loc.hpp>
#include <yactfr/metadata/int-range-set.hpp>
#include <yactfr/metadata/fl-int-type.hpp>
#include <yactfr/metadata/vl-int-type.hpp>
#include <yactfr/metadata/str-type.hpp>
#include <yactfr/metadata/trace-type.hpp>
#include <yactfr/metadata/trace-env.hpp>
#include <yactfr/metadata/aliases.hpp>
#include <yactfr/text-loc.hpp>

namespace yactfr {
namespace internal {

/*
 * All the "pseudo" stuff below exists because the yactfr metadata
 * objects are immutable and we need to modify them sometimes during the
 * parsing process, mostly because of data type aliases containing
 * relative data locations (they need to be converted to absolute data
 * locations because this is what the yactfr API expects).
 */

/*
 * A pseudo data location is the result of parsing a data location,
 * possibly not yet converted to an absolute data location (as yactfr
 * requires).
 *
 * The kinds of pseudo data locations are:
 *
 * `Kind::Env`:
 *     The parsed data location (TSDL) was `env.SOMETHING`, where
 *     `SOMETHING` must be an existing environment key (for
 *     static-length array types). The first element of `pathElems` is
 *     `SOMETHING` in this case.
 *
 * `Kind::Abs`:
 *     Absolute (has a scope).
 *
 *     `scope` indicates the root scope, and `pathElems` contains
 *     everything else (`stream.packet.context` and so forth are
 *     stripped for TSDL).
 *
 * `Kind::Rel1`::
 *     Relative from CTF 1.8.
 *
 *     All path elements have values.
 *
 * `Kind::Rel2`::
 *     Relative from CTF 2.
 *
 *     A path element may be `boost::none` to indicate "parent".
 *
 * The distinction between `Kind::Rel1` and `Kind::Rel2` is important
 * because `Kind::Rel2` allows no-value path elements (`boost::none`)
 * to explicitly indicate "parent", whereas `Kind::Rel1` indicates
 * "parent" implicitly.
 */
class PseudoDataLoc final
{
public:
    // `boost::none` explicitly means "parent" here (CTF 2)
    using PathElems = std::vector<boost::optional<std::string>>;

    enum class Kind {
        Env,    // environment (CTF 1.8)
        Abs,    // absolute
        Rel1,   // relative from CTF 1.8
        Rel2,   // relative from CTF 2
    };

public:
    explicit PseudoDataLoc(Kind kind, boost::optional<Scope> scope, PathElems pathElems,
                           TextLocation loc);

    PseudoDataLoc(const PseudoDataLoc&) = default;
    PseudoDataLoc(PseudoDataLoc&&) = default;
    PseudoDataLoc& operator=(const PseudoDataLoc&) = default;
    PseudoDataLoc& operator=(PseudoDataLoc&&) = default;

    Kind kind() const noexcept
    {
        return _kind;
    }

    const boost::optional<Scope>& scope() const noexcept
    {
        return _scope;
    }

    const PathElems& pathElems() const noexcept
    {
        return _pathElems;
    }

    const TextLocation& loc() const noexcept
    {
        return _loc;
    }

private:
    Kind _kind;
    boost::optional<Scope> _scope;
    PathElems _pathElems;
    TextLocation _loc;
};

class PseudoDt;
class PseudoDtVisitor;
class ConstPseudoDtVisitor;

/*
 * Base pseudo data type.
 */
class PseudoDt :
    boost::noncopyable
{
public:
    using Up = std::unique_ptr<PseudoDt>;

    enum class Kind
    {
        ScalarDtWrapper,
        FlUInt,
        SlArray,
        DlArray,
        DlBlob,
        Struct,
        Var,
        VarWithIntRanges,
        Opt,
        OptWithBoolSel,
        OptWithIntSel,
    };

protected:
    explicit PseudoDt(TextLocation loc);

public:
    virtual ~PseudoDt() = default;
    virtual Kind kind() const noexcept = 0;
    virtual bool isEmpty() const;
    virtual void accept(PseudoDtVisitor& visitor) = 0;
    virtual void accept(ConstPseudoDtVisitor& visitor) const = 0;
    virtual bool isInt() const noexcept;
    virtual bool isUInt() const noexcept;
    virtual bool isFlUInt() const noexcept;

    /*
     * Fully clones this pseudo data type, meaning the returned object
     * is completely independent from this one.
     */
    virtual PseudoDt::Up clone() const = 0;

    const TextLocation& loc() const noexcept
    {
        return _loc;
    }

    void loc(TextLocation loc) noexcept
    {
        _loc = std::move(loc);
    }

    const boost::optional<Index>& posInScope() const noexcept
    {
        return _posInScope;
    }

    void posInScope(const Index posInScope) noexcept
    {
        _posInScope = posInScope;
    }

private:
    TextLocation _loc;

    /*
     * Numeric position of this pseudo data type within its root scope.
     *
     * This is used during a length/selector pseudo data type lookup to
     * validate that the target will be decoded before the source.
     *
     * Set by setPseudoDtPosInScope().
     */
    boost::optional<Index> _posInScope;
};

using PseudoDtSet = std::unordered_set<PseudoDt *>;
using ConstPseudoDtSet = std::unordered_set<const PseudoDt *>;

/*
 * This is a wrapper for any yactfr scalar type, except unsigned integer
 * types.
 *
 * Scalar types are final and don't need to be modified during the
 * parsing process, so we can simply create them directly, wrap them
 * within instances of `PseudoScalarDtWrapper` so that they get the
 * common pseudo data type base, and move them later without unnecessary
 * allocations.
 */
class PseudoScalarDtWrapper :
    public PseudoDt
{
public:
    explicit PseudoScalarDtWrapper(DataType::Up dt,
                                   boost::optional<StringEncoding> encoding = boost::none,
                                   TextLocation loc = TextLocation {});

    explicit PseudoScalarDtWrapper(DataType::Up dt, TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::ScalarDtWrapper;
    }

    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;
    PseudoDt::Up clone() const override;
    bool isInt() const noexcept override;
    bool isUInt() const noexcept override;
    bool isFlUInt() const noexcept override;
    bool isEmpty() const override;

    const DataType& dt() const noexcept
    {
        return *_dt;
    }

    const boost::optional<StringEncoding>& encoding() const noexcept
    {
        return _encoding;
    }

private:
    DataType::Up _dt;
    boost::optional<StringEncoding> _encoding;
};

/*
 * Mixin to add attribute property.
 */
class WithAttrsMixin
{
public:
    explicit WithAttrsMixin() = default;
    explicit WithAttrsMixin(MapItem::Up attrs);

    const MapItem *attrs() const noexcept
    {
        return _attrs.get();
    }

private:
    MapItem::Up _attrs;
};

/*
 * Pseudo fixed-length unsigned integer type.
 *
 * This is needed because:
 *
 * * During the decoding process, a TSDL fixed-length unsigned integer
 *   type may be mapped to a clock type by internal ID, and we want to
 *   keep this ID for validation and role creation purposes.
 *
 * * A TSDL fixed-length unsigned integer type may have an implicit
 *   role, but we need its structure member type name to assign it.
 *
 * * A TSDL fixed-length unsigned integer type may have an encoding:
 *   this is only needed to detect static-length and dynamic-length
 *   string types; a yactfr fixed-length integer type has no encoding.
 *
 * Keep an unmapped fixed-length unsigned integer type here as well as
 * the _internal ID_ of the mapped clock type, if any.
 */
class PseudoFlUIntType :
    public PseudoDt,
    public WithAttrsMixin
{
public:
    explicit PseudoFlUIntType(unsigned int align, unsigned int len, ByteOrder bo,
                              const BitOrder bio, DisplayBase prefDispBase,
                              FixedLengthUnsignedIntegerType::Mappings mappings,
                              boost::optional<StringEncoding> encoding = boost::none,
                              boost::optional<std::string> mappedClkTypeId = boost::none,
                              MapItem::Up attrs = nullptr, UnsignedIntegerTypeRoleSet roles = {},
                              TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::FlUInt;
    }

    PseudoDt::Up clone() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;
    bool isInt() const noexcept override;
    bool isUInt() const noexcept override;
    bool isFlUInt() const noexcept override;

    unsigned int align() const noexcept
    {
        return _align;
    }

    unsigned int len() const noexcept
    {
        return _len;
    }

    ByteOrder bo() const noexcept
    {
        return _bo;
    }

    BitOrder bio() const noexcept
    {
        return _bio;
    }

    DisplayBase prefDispBase() const noexcept
    {
        return _prefDispBase;
    }

    const boost::optional<StringEncoding>& encoding() const noexcept
    {
        return _encoding;
    }

    const boost::optional<std::string>& mappedClkTypeId() const noexcept
    {
        return _mappedClkTypeId;
    }

    void mappedClkTypeId(std::string id) noexcept
    {
        _mappedClkTypeId = std::move(id);
    }

    const UnsignedIntegerTypeRoleSet& roles() const noexcept
    {
        return _roles;
    }

    void addRole(const UnsignedIntegerTypeRole role)
    {
        _roles.insert(role);
    }

    bool hasRole(const UnsignedIntegerTypeRole role) const noexcept
    {
        return _roles.find(role) != _roles.end();
    }

    const FixedLengthUnsignedIntegerType::Mappings& mappings() const noexcept
    {
        return _mappings;
    }

private:
    unsigned int _align;
    unsigned int _len;
    ByteOrder _bo;
    BitOrder _bio;
    DisplayBase _prefDispBase;
    FixedLengthUnsignedIntegerType::Mappings _mappings;
    boost::optional<StringEncoding> _encoding;
    boost::optional<std::string> _mappedClkTypeId;
    UnsignedIntegerTypeRoleSet _roles;
};

/*
 * Pseudo static-length data type mixin.
 */
class PseudoSlDtMixin
{
protected:
    explicit PseudoSlDtMixin(Size len);

public:
    Size len() const noexcept
    {
        return _len;
    }

protected:
    Size _len;
};

/*
 * Pseudo dynamic-length data type mixin.
 */
class PseudoDlDtMixin
{
protected:
    explicit PseudoDlDtMixin(PseudoDataLoc pseudoLenLoc);

public:
    const PseudoDataLoc& pseudoLenLoc() const noexcept
    {
        return _pseudoLenLoc;
    }

    const boost::optional<DataLocation>& lenLoc() const noexcept
    {
        return _lenLoc;
    }

    void lenLoc(DataLocation&& lenLoc) noexcept
    {
        _lenLoc = std::move(lenLoc);
    }

protected:
    PseudoDataLoc _pseudoLenLoc;

    // set by setPseudoDtDataLoc() from `_pseudoLenLoc`
    boost::optional<DataLocation> _lenLoc;
};

/*
 * Pseudo array type (base class).
 */
struct PseudoArrayType :
    public PseudoDt,
    public WithAttrsMixin
{
protected:
    explicit PseudoArrayType(unsigned int minAlign, PseudoDt::Up pseudoElemType,
                             MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

public:
    PseudoDt& pseudoElemType() noexcept
    {
        return *_pseudoElemType;
    }

    const PseudoDt& pseudoElemType() const noexcept
    {
        return *_pseudoElemType;
    }

    unsigned int minAlign() const noexcept
    {
        return _minAlign;
    }

private:
    unsigned int _minAlign;
    PseudoDt::Up _pseudoElemType;
};

/*
 * Pseudo static-length array type.
 */
class PseudoSlArrayType final :
    public PseudoArrayType,
    public PseudoSlDtMixin
{
public:
    explicit PseudoSlArrayType(unsigned int minAlign, Size len, PseudoDt::Up pseudoElemType,
                               MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

    explicit PseudoSlArrayType(Size len, PseudoDt::Up pseudoElemType,
                               MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::SlArray;
    }

    PseudoDt::Up clone() const override;
    bool isEmpty() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;

    bool hasMetadataStreamUuidRole() const noexcept
    {
        return _hasMetadataStreamUuidRole;
    }

    void hasMetadataStreamUuidRole(const bool hasMetadataStreamUuidRole) noexcept
    {
        _hasMetadataStreamUuidRole = hasMetadataStreamUuidRole;
    }

private:
    bool _hasMetadataStreamUuidRole = false;
};

/*
 * Pseudo dynamic-length array type.
 *
 * `pseudoLenLoc` may be a relative data location.
 */
class PseudoDlArrayType final :
    public PseudoArrayType,
    public PseudoDlDtMixin
{
public:
    explicit PseudoDlArrayType(unsigned int minAlign, PseudoDataLoc pseudoLenLoc,
                               PseudoDt::Up pseudoElemType, MapItem::Up attrs = nullptr,
                               TextLocation loc = TextLocation {});

    explicit PseudoDlArrayType(PseudoDataLoc pseudoLenLoc, PseudoDt::Up pseudoElemType,
                               MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::DlArray;
    }

    PseudoDt::Up clone() const override;
    bool isEmpty() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;
};

/*
 * Pseudo BLOB type (base class).
 */
struct PseudoBlobType :
    public PseudoDt,
    public WithAttrsMixin
{
protected:
    explicit PseudoBlobType(boost::optional<std::string> mediaType,
                            MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

public:
    const boost::optional<std::string>& mediaType() const noexcept
    {
        return _mediaType;
    }

private:
    boost::optional<std::string> _mediaType;
    PseudoDt::Up _pseudoElemType;
};

/*
 * Pseudo dynamic-length BLOB type.
 *
 * `pseudoLenLoc` may be a relative data location.
 */
class PseudoDlBlobType final :
    public PseudoBlobType,
    public PseudoDlDtMixin
{
public:
    explicit PseudoDlBlobType(PseudoDataLoc pseudoLenLoc, boost::optional<std::string> mediaType,
                              MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::DlBlob;
    }

    PseudoDt::Up clone() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;
};

/*
 * Pseudo named data type.
 */
class PseudoNamedDt final :
    public WithAttrsMixin
{
public:
    using Up = std::unique_ptr<PseudoNamedDt>;

public:
    explicit PseudoNamedDt() = default;

    explicit PseudoNamedDt(boost::optional<std::string> name, PseudoDt::Up pseudoDt,
                           MapItem::Up attrs = nullptr);

    const boost::optional<std::string>& name() const noexcept
    {
        return _name;
    }

    const PseudoDt& pseudoDt() const noexcept
    {
        return *_pseudoDt;
    }

    PseudoDt& pseudoDt() noexcept
    {
        return *_pseudoDt;
    }

private:
    boost::optional<std::string> _name;
    PseudoDt::Up _pseudoDt;
};

using PseudoNamedDts = std::vector<PseudoNamedDt::Up>;

/*
 * Pseudo structure type.
 */
class PseudoStructType final :
    public PseudoDt,
    public WithAttrsMixin
{
public:
    explicit PseudoStructType(unsigned int minAlign, PseudoNamedDts&& pseudoMemberTypes,
                              MapItem::Up attrs = nullptr, TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::Struct;
    }

    PseudoDt::Up clone() const override;
    bool isEmpty() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;

    /*
     * Returns the member class named `name`, or `nullptr` if not found.
     */
    const PseudoNamedDt *operator[](const std::string& name) const noexcept;

    unsigned int minAlign() const noexcept
    {
        return _minAlign;
    }

    PseudoNamedDts& pseudoMemberTypes() noexcept
    {
        return _pseudoMemberTypes;
    }

    const PseudoNamedDts& pseudoMemberTypes() const noexcept
    {
        return _pseudoMemberTypes;
    }

private:
    unsigned int _minAlign;
    PseudoNamedDts _pseudoMemberTypes;
};

/*
 * Pseudo variant type.
 *
 * `pseudoSelLoc` may be a relative data location.
 */
class PseudoVarType :
    public PseudoDt,
    public WithAttrsMixin
{
public:
    explicit PseudoVarType(boost::optional<PseudoDataLoc> pseudoSelLoc,
                           PseudoNamedDts&& pseudoOpts, MapItem::Up attrs = nullptr,
                           TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::Var;
    }

    PseudoDt::Up clone() const override;
    bool isEmpty() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;

    const boost::optional<PseudoDataLoc>& pseudoSelLoc() const noexcept
    {
        return _pseudoSelLoc;
    }

    void pseudoSelLoc(PseudoDataLoc loc) noexcept
    {
        _pseudoSelLoc = std::move(loc);
    }

    const boost::optional<DataLocation>& selLoc() const noexcept
    {
        return _selLoc;
    }

    void selLoc(DataLocation&& selLoc) noexcept
    {
        _selLoc = std::move(selLoc);
    }

    PseudoNamedDts& pseudoOpts() noexcept
    {
        return _pseudoOpts;
    }

    const PseudoNamedDts& pseudoOpts() const noexcept
    {
        return _pseudoOpts;
    }

protected:
    PseudoNamedDts _clonePseudoOpts() const;

private:
    PseudoNamedDts _pseudoOpts;
    boost::optional<PseudoDataLoc> _pseudoSelLoc;

    // set by setPseudoDtDataLoc() from `_pseudoSelLoc`
    boost::optional<DataLocation> _selLoc;
};

/*
 * Pseudo variant (with integer ranges) type.
 *
 * `pseudoSelLoc` may be a relative data location.
 */
class PseudoVarWithIntRangesType final :
    public PseudoVarType
{
public:
    /*
     * This is a hack: such an integer range set may in fact contain
     * signed integer ranges, but we'll only know in
     * DtFromPseudoRootDtConverter::_dtFromPseudoVarWithIntRangesType(),
     * at which point signed integer range sets will be created, casting
     * the lower and upper values to `long long`.
     *
     * Hackish, but safe.
     */
    using RangeSets = std::vector<IntegerRangeSet<unsigned long long, false>>;

public:
    /*
     * `ranges` matches `pseudoOpts`, that is, `ranges[i]` is the
     * corresponding integer range set of the option `pseudoOpts[i]`.
     */
    explicit PseudoVarWithIntRangesType(boost::optional<PseudoDataLoc> pseudoSelLoc,
                                        PseudoNamedDts&& pseudoOpts, RangeSets&& rangeSets,
                                        MapItem::Up attrs = nullptr,
                                        TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::VarWithIntRanges;
    }

    PseudoDt::Up clone() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;

    const RangeSets& rangeSets() const noexcept
    {
        return _rangeSets;
    }

private:
    RangeSets _rangeSets;
};

/*
 * Pseudo optional type.
 *
 * `pseudoSelLoc` may be a relative data location.
 */
class PseudoOptType :
    public PseudoDt,
    public WithAttrsMixin
{
protected:
    explicit PseudoOptType(PseudoDt::Up pseudoDt, PseudoDataLoc&& pseudoSelLoc,
                           MapItem::Up attrs, TextLocation&& loc);

public:
    bool isEmpty() const override;

    PseudoDt& pseudoDt() noexcept
    {
        return *_pseudoDt;
    }

    const PseudoDt& pseudoDt() const noexcept
    {
        return *_pseudoDt;
    }

    const PseudoDataLoc& pseudoSelLoc() const noexcept
    {
        return _pseudoSelLoc;
    }

    const boost::optional<DataLocation>& selLoc() const noexcept
    {
        return _selLoc;
    }

    void selLoc(DataLocation&& selLoc) noexcept
    {
        _selLoc = std::move(selLoc);
    }

private:
    PseudoDt::Up _pseudoDt;
    PseudoDataLoc _pseudoSelLoc;

    // set by setPseudoDtDataLoc() from `_pseudoSelLoc`
    boost::optional<DataLocation> _selLoc;
};

/*
 * Pseudo optional (with boolean selector) type.
 *
 * `pseudoSelLoc` may be a relative data location.
 */
class PseudoOptWithBoolSelType :
    public PseudoOptType
{
public:
    explicit PseudoOptWithBoolSelType(PseudoDt::Up pseudoDt, PseudoDataLoc pseudoSelLoc,
                                      MapItem::Up attrs = nullptr,
                                      TextLocation loc = TextLocation {});

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::OptWithBoolSel;
    }

    PseudoDt::Up clone() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;
};

/*
 * Pseudo optional (with integer selector) type.
 *
 * `pseudoSelLoc` may be a relative data location.
 */
class PseudoOptWithIntSelType :
    public PseudoOptType
{
public:
    /*
     * This is a hack: such an integer range set may in fact contain
     * signed integer ranges, but we'll only know in
     * DtFromPseudoRootDtConverter::_dtFromPseudoOptWithIntSelType(), at
     * which point a signed integer range set will be created, casting
     * the lower and upper values to `long long`.
     *
     * Hackish, but safe.
     */
    using RangeSet = IntegerRangeSet<unsigned long long, false>;

public:
    explicit PseudoOptWithIntSelType(PseudoDt::Up pseudoDt, PseudoDataLoc pseudoSelLoc,
                                     RangeSet&& selRanges, MapItem::Up attrs = nullptr,
                                     TextLocation loc = TextLocation {});

    const RangeSet& selRanges() const noexcept
    {
        return _selRanges;
    }

    PseudoDt::Kind kind() const noexcept override
    {
        return PseudoDt::Kind::OptWithIntSel;
    }

    PseudoDt::Up clone() const override;
    void accept(PseudoDtVisitor& visitor) override;
    void accept(ConstPseudoDtVisitor& visitor) const override;

private:
    RangeSet _selRanges;
};

class PseudoDst;

/*
 * Pseudo event record type: mutable event record type.
 */
class PseudoErt final :
    public WithAttrsMixin
{
public:
    explicit PseudoErt(TypeId id, boost::optional<std::string> ns,
                       boost::optional<std::string> name, boost::optional<std::string> uid,
                       boost::optional<LogLevel> logLevel, boost::optional<std::string> emfUri,
                       PseudoDt::Up pseudoSpecCtxType, PseudoDt::Up pseudoPayloadType,
                       MapItem::Up attrs = nullptr);

    PseudoErt(const PseudoErt&) = delete;
    PseudoErt(PseudoErt&&) = default;
    PseudoErt& operator=(const PseudoErt&) = delete;
    PseudoErt& operator=(PseudoErt&&) = default;

    /*
     * Validates this pseudo event record type, as belonging to
     * `pseudoDst`, throwing `TextParseError` on any error.
     */
    void validate(const PseudoDst& pseudoDst) const;

    TypeId id() const noexcept
    {
        return _id;
    }

    const boost::optional<std::string>& ns() const noexcept
    {
        return _ns;
    }

    const boost::optional<std::string>& name() const noexcept
    {
        return _name;
    }

    const boost::optional<std::string>& uid() const noexcept
    {
        return _uid;
    }

    const boost::optional<LogLevel>& logLevel() const noexcept
    {
        return _logLevel;
    }

    const boost::optional<std::string>& emfUri() const noexcept
    {
        return _emfUri;
    }

    const PseudoDt *pseudoSpecCtxType() const noexcept
    {
        return _pseudoSpecCtxType.get();
    }

    PseudoDt *pseudoSpecCtxType() noexcept
    {
        return _pseudoSpecCtxType.get();
    }

    const PseudoDt *pseudoPayloadType() const noexcept
    {
        return _pseudoPayloadType.get();
    }

    PseudoDt *pseudoPayloadType() noexcept
    {
        return _pseudoPayloadType.get();
    }

private:
    void _validateNotEmpty(const PseudoDst& pseudoDst) const;
    void _validateNoMappedClkTypeId() const;

private:
    TypeId _id = 0;
    boost::optional<std::string> _ns;
    boost::optional<std::string> _name;
    boost::optional<std::string> _uid;
    boost::optional<LogLevel> _logLevel;
    boost::optional<std::string> _emfUri;
    PseudoDt::Up _pseudoSpecCtxType;
    PseudoDt::Up _pseudoPayloadType;
};

/*
 * Set of pseudo event record types.
 */
using PseudoErtSet = std::unordered_set<PseudoErt *>;

/*
 * Pseudo data stream type: mutable data stream type.
 */
class PseudoDst final :
    public WithAttrsMixin
{
public:
    explicit PseudoDst() = default;
    explicit PseudoDst(TypeId id, boost::optional<std::string> ns,
                       boost::optional<std::string> name, boost::optional<std::string> uid,
                       PseudoDt::Up pseudoPktCtxType, PseudoDt::Up pseudoErHeaderType,
                       PseudoDt::Up pseudoErCommonCtxType, const ClockType *defClkType = nullptr,
                       MapItem::Up attrs = nullptr);

    PseudoDst(const PseudoDst&) = delete;
    PseudoDst(PseudoDst&&) = default;
    PseudoDst& operator=(const PseudoDst&) = delete;
    PseudoDst& operator=(PseudoDst&&) = default;

    /*
     * Validates this pseudo data stream type, as containing the pseudo
     * event record types `pseudoErts`, throwing `TextParseError` on
     * any error.
     */
    void validate(const PseudoErtSet& pseudoErts) const;

    TypeId id() const noexcept
    {
        return _id;
    }

    const boost::optional<std::string>& ns() const noexcept
    {
        return _ns;
    }

    const boost::optional<std::string>& name() const noexcept
    {
        return _name;
    }

    const boost::optional<std::string>& uid() const noexcept
    {
        return _uid;
    }

    const PseudoDt *pseudoPktCtxType() const noexcept
    {
        return _pseudoPktCtxType.get();
    }

    PseudoDt *pseudoPktCtxType() noexcept
    {
        return _pseudoPktCtxType.get();
    }

    const PseudoDt *pseudoErHeaderType() const noexcept
    {
        return _pseudoErHeaderType.get();
    }

    PseudoDt *pseudoErHeaderType() noexcept
    {
        return _pseudoErHeaderType.get();
    }

    const PseudoDt *pseudoErCommonCtxType() const noexcept
    {
        return _pseudoErCommonCtxType.get();
    }

    PseudoDt *pseudoErCommonCtxType() noexcept
    {
        return _pseudoErCommonCtxType.get();
    }

    const ClockType *defClkType() const noexcept
    {
        return _defClkType;
    }

    void defClkType(const ClockType& clkType) noexcept
    {
        _defClkType = &clkType;
    }

private:
    void _validatePktCtxType() const;
    void _validateErHeaderType(const PseudoErtSet& pseudoErts) const;
    void _validateErCommonCtxType() const;
    void _validateNoMappedClkTypeId() const;

private:
    TypeId _id = 0;
    boost::optional<std::string> _ns;
    boost::optional<std::string> _name;
    boost::optional<std::string> _uid;
    PseudoDt::Up _pseudoPktCtxType;
    PseudoDt::Up _pseudoErHeaderType;
    PseudoDt::Up _pseudoErCommonCtxType;
    const ClockType *_defClkType = nullptr;
};

/*
 * Orphan pseudo event record type entry.
 */
class PseudoOrphanErt final
{
public:
    explicit PseudoOrphanErt(PseudoErt pseudoErt, TextLocation loc);
    PseudoOrphanErt(const PseudoOrphanErt&) = delete;
    PseudoOrphanErt(PseudoOrphanErt&&) = default;
    PseudoOrphanErt& operator=(const PseudoOrphanErt&) = delete;
    PseudoOrphanErt& operator=(PseudoOrphanErt&&) = default;

    const PseudoErt& pseudoErt() const noexcept
    {
        return _pseudoErt;
    }

    PseudoErt& pseudoErt() noexcept
    {
        return _pseudoErt;
    }

    const TextLocation& loc() const noexcept
    {
        return _loc;
    }

private:
    PseudoErt _pseudoErt;
    TextLocation _loc;
};

/*
 * Pseudo trace type: mutable trace type.
 */
class PseudoTraceType final :
    public WithAttrsMixin
{
public:
    using PseudoDsts = std::unordered_map<TypeId, std::unique_ptr<PseudoDst>>;
    using PseudoOrphanErts = std::unordered_map<TypeId, std::unordered_map<TypeId, PseudoOrphanErt>>;

public:
    explicit PseudoTraceType(unsigned int majorVersion, unsigned int minorVersion,
                             boost::optional<std::string> ns = boost::none,
                             boost::optional<std::string> name = boost::none,
                             boost::optional<std::string> uid = boost::none,
                             TraceEnvironment env = TraceEnvironment {},
                             PseudoDt::Up pseudoPktHeaderType = nullptr,
                             MapItem::Up attrs = nullptr);

    /*
     * Validates this pseudo trace type, throwing `TextParseError`
     * on any error.
     */
    void validate() const;

    unsigned int majorVersion() const noexcept
    {
        return _majorVersion;
    }

    unsigned int minorVersion() const noexcept
    {
        return _minorVersion;
    }

    const boost::optional<std::string>& ns() const noexcept
    {
        return _ns;
    }

    const boost::optional<std::string>& name() const noexcept
    {
        return _name;
    }

    const boost::optional<std::string>& uid() const noexcept
    {
        return _uid;
    }

    const TraceEnvironment& env() const noexcept
    {
        return _env;
    }

    void env(TraceEnvironment env) noexcept
    {
        _env = std::move(env);
    }

    const PseudoDt *pseudoPktHeaderType() const noexcept
    {
        return _pseudoPktHeaderType.get();
    }

    PseudoDt *pseudoPktHeaderType() noexcept
    {
        return _pseudoPktHeaderType.get();
    }

    const ClockTypeSet& clkTypes() const noexcept
    {
        return _clkTypes;
    }

    ClockTypeSet& clkTypes() noexcept
    {
        return _clkTypes;
    }

    bool hasClkType(const std::string& id) const noexcept;
    const ClockType *findClkType(const std::string& id) const noexcept;

    const PseudoDsts& pseudoDsts() const noexcept
    {
        return _pseudoDsts;
    }

    PseudoDsts& pseudoDsts() noexcept
    {
        return _pseudoDsts;
    }

    bool hasPseudoDst(TypeId id) const noexcept;

    const PseudoOrphanErts& pseudoOrphanErts() const noexcept
    {
        return _pseudoOrphanErts;
    }

    PseudoOrphanErts& pseudoOrphanErts() noexcept
    {
        return _pseudoOrphanErts;
    }

    bool hasPseudoOrphanErt(TypeId dstId, TypeId ertId) const noexcept;

private:
    unsigned int _majorVersion;
    unsigned int _minorVersion;
    boost::optional<std::string> _ns;
    boost::optional<std::string> _name;
    boost::optional<std::string> _uid;
    TraceEnvironment _env;
    PseudoDt::Up _pseudoPktHeaderType;
    ClockTypeSet _clkTypes;
    PseudoDsts _pseudoDsts;
    PseudoOrphanErts _pseudoOrphanErts;
};

} // namespace internal
} // namespace yactfr

#endif // YACTFR_INTERNAL_METADATA_PSEUDO_TYPES_HPP
