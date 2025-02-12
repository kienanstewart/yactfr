/*
 * Copyright (C) 2015-2018 Philippe Proulx <eepp.ca>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sstream>
#include <functional>
#include <cstdlib>
#include <cstring>

#ifndef NDEBUG
# include <iostream>
#endif

#include <yactfr/metadata/struct-type.hpp>
#include <yactfr/metadata/trace-type.hpp>
#include <yactfr/metadata/data-loc.hpp>
#include <yactfr/aliases.hpp>

#include "../internal/metadata/trace-type-impl.hpp"
#include "../internal/proc.hpp"
#include "../internal/pkt-proc-builder.hpp"

namespace yactfr {

TraceType::TraceType(const unsigned int majorVersion, const unsigned int minorVersion,
                     boost::optional<std::string> ns, boost::optional<std::string> name,
                     boost::optional<std::string> uid, TraceEnvironment env,
                     StructureType::Up pktHeaderType, ClockTypeSet&& clkTypes,
                     DataStreamTypeSet&& dsts, MapItem::Up attrs) :
    _pimpl {
        std::make_unique<internal::TraceTypeImpl>(majorVersion, minorVersion, std::move(ns),
                                                  std::move(name), std::move(uid), std::move(env),
                                                  std::move(pktHeaderType), std::move(clkTypes),
                                                  std::move(dsts), std::move(attrs), *this)
    }
{
#ifndef NDEBUG
    const auto var = std::getenv("YACTFR_DEBUG_PRINT_PROC");

    if (var && std::strcmp(var, "1") == 0) {
        auto& pktProc = _pimpl->pktProc();
        std::cout << pktProc.toStr(0) << std::endl;
    }
#endif
}

TraceType::~TraceType()
{
}

unsigned int TraceType::majorVersion() const noexcept
{
    return _pimpl->majorVersion();
}

unsigned int TraceType::minorVersion() const noexcept
{
    return _pimpl->minorVersion();
}

const boost::optional<std::string>& TraceType::nameSpace() const noexcept
{
    return _pimpl->ns();
}

const boost::optional<std::string>& TraceType::name() const noexcept
{
    return _pimpl->name();
}

const boost::optional<std::string>& TraceType::uid() const noexcept
{
    return _pimpl->uid();
}

const TraceEnvironment& TraceType::environment() const noexcept
{
    return _pimpl->environment();
}

const StructureType* TraceType::packetHeaderType() const noexcept
{
    return _pimpl->pktHeaderType();
}

const ClockTypeSet& TraceType::clockTypes() const noexcept
{
    return _pimpl->clkTypes();
}

const DataStreamTypeSet& TraceType::dataStreamTypes() const noexcept
{
    return _pimpl->dsts();
}

const MapItem *TraceType::attributes() const noexcept
{
    return _pimpl->attrs();
}

const DataStreamType *TraceType::operator[](const TypeId id) const
{
    return _pimpl->findDst(id);
}

DataStreamTypeSet::const_iterator TraceType::begin() const noexcept
{
    return _pimpl->dsts().begin();
}

DataStreamTypeSet::const_iterator TraceType::end() const noexcept
{
    return _pimpl->dsts().end();
}

Size TraceType::size() const noexcept
{
    return _pimpl->dsts().size();
}

bool TraceType::isEmpty() const noexcept
{
    return _pimpl->dsts().empty();
}

} // namespace yactfr
