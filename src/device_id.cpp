#include "device_id.hpp"

#include "log.hpp"
#include "util.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <algorithm>
#include <ranges>
#include <set>

namespace device_id
{

namespace syntax
{

// BracketRange ///////////////////////////////////////////////////////////////

// marcinw:TODO: move
BracketRange::BracketRange(const DeviceIndex& left, const DeviceIndex& right) :
    left(left), right(right)
{
    if (left > right)
    {
        // marcinw:TODO: error message
        throw std::runtime_error("BracketRange: ! left > right");
    }
}

// BracketRangeMap ////////////////////////////////////////////////////////////

BracketRangeMap::BracketRangeMap(BracketRange&& from, BracketRange&& to) :
    from(std::move(from)), to(std::move(to))
{
    if (!(from.size() == to.size() || to.size() == 1))
    {
        // marcinw:TODO: error message
        throw std::runtime_error("BracketRangeMap:"
                                 " from.size() == to.size() || to.size() == 1");
    }
}

BracketRangeMap::operator BracketMap() const
{
    BracketMap result;
    if (to.size() == 1)
    {
        const auto& value = *to.begin();
        for (const auto& key : from)
        {
            result[key] = value;
        }
    }
    else
    {
        for (auto keysIt = from.begin(), valuesIt = to.begin();
             keysIt != from.end(); ++keysIt, ++valuesIt)
        {
            result[*keysIt] = *valuesIt;
        }
    }
    return result;
}

// IndexedBracketMap //////////////////////////////////////////////////////////

const int IndexedBracketMap::indexPosImplicit = -1;

bool IndexedBracketMap::isImplicit() const
{
    return inputPosition == indexPosImplicit;
}
void IndexedBracketMap::setInputPosition(unsigned pos)
{
    inputPosition = pos;
}

int IndexedBracketMap::getInputPosition() const
{
    return inputPosition;
}

BracketMap IndexedBracketMap::map() const
{
    return indexMap;
}

} // namespace syntax

const int PatternIndex::unspecified = -1;

// PatternIndex ///////////////////////////////////////////////////////////////

unsigned PatternIndex::dim() const
{
    return indexes.size();
}

int PatternIndex::operator[](unsigned i) const
{
    return i < indexes.size() ? indexes.at(i) : unspecified;
}

bool PatternIndex::operator==(const PatternIndex& other) const
{
    return this == &other || this->indexes == other.indexes;
}

/**
 * Ordering of pattern indexes are analogous to the ordering of natural numbers
 * viewed from their positional encoding perspective.
 */
bool PatternIndex::operator<(const PatternIndex& other) const
{
    for (unsigned i = 0; i < std::max(this->dim(), other.dim()); ++i)
    {
        // "unspecified" as the negative number is lesser than any specified
        // value
        if ((*this)[i] < other[i])
        {
            return true;
        }
        else if ((*this)[i] > other[i])
        {
            return false;
        }
    }
    // If none of the digits on any position are neither lesser nor greater then
    // they're all equal, which means (*this) == other
    return false;
}

void PatternIndex::set(unsigned i, int value)
{
    int actualValue = value >= 0 ? value : unspecified;
    if (indexes.size() <= i)
    {
        if (actualValue != unspecified)
        {
            indexes.resize(i + 1, unspecified);
            indexes[i] = actualValue;
        }
    }
    else // indexes.size() > i
    {
        indexes[i] = actualValue;
        normalize();
    }
}

void PatternIndex::normalize()
{
    while (indexes.size() > 0 && indexes.back() < 0)
    {
        indexes.pop_back();
    }
}

// CartesianProductRange //////////////////////////////////////////////////////

PatternIndex CartesianProductRange::iterator_t::operator*()
{
    // masterIndex(k) =
    // res[0] * limits[1] * limits[2] * ... * limits[k-1] +
    // res[1] * limits[2] * ... * limits[k-1] +
    // ...
    // res[k-3] * limits[k-2] * limits[k-1] +
    // res[k-2] * limits[k-1] +
    // res[k-1]
    //
    // masterIndex(k) * limits[k] + res[k] =
    // res[0] * limits[1] * limits[2] * ... * limits[k-1] * limits[k] +
    // res[1] * limits[2] * ... * limits[k-1] * limits[k] +
    // ...
    // res[k-3] * limits[k-2] * limits[k-1] * limits[k] +
    // res[k-2] * limits[k-1] * limits[k] +
    // res[k-1] * limits[k]
    // res[k]
    //
    // masterIndex(k) * limits[k] + res[k] = masterIndex(k+1)
    // masterIndex(k-1) * limits[k-1] + res[k-1] = masterIndex(k)
    //
    // masterIndex(k) = masterIndex(k-1) * limits[k-1] + res[k-1]
    // masterIndex(0) = 0
    //
    // while:
    // 0 <= res[k-1] < limits[k-1]
    //
    // =>
    // res[k-1] = masterIndex(k) % limits[k-1]
    // masterIndex(k-1) = floor(masterIndex(k) / limits[k-1])

    PatternIndex result;
    unsigned mi = masterIndex;
    if (ranges.size() > 0)
    {
        for (int i = ranges.size() - 1; i >= 0; --i)
        {
            auto s = ranges[i].size();
            result.set(i, ranges[i][mi % s]);
            mi = mi / s;
        }
    }
    return result;
}

CartesianProductRange::iterator_t&
    CartesianProductRange::iterator_t::operator++()
{
    ++masterIndex;
    return *this;
}

bool CartesianProductRange::iterator_t::operator==(
    const iterator_t& other) const
{
    return this == &other || (&this->ranges == &other.ranges &&
                              this->masterIndex == other.masterIndex);
}

bool CartesianProductRange::iterator_t::operator!=(
    const iterator_t& other) const
{
    return !(*this == other);
}

CartesianProductRange::iterator_t CartesianProductRange::begin() const
{
    return cbegin();
}

CartesianProductRange::iterator_t CartesianProductRange::end() const
{
    return cend();
}

CartesianProductRange::iterator_t CartesianProductRange::cbegin() const
{
    return iterator_t(0, ranges);
}

CartesianProductRange::iterator_t CartesianProductRange::cend() const
{
    return iterator_t(size(), ranges);
}

unsigned CartesianProductRange::size() const
{
    return product(ranges);
}

unsigned CartesianProductRange::product(
    const std::vector<PatternInputMapping>& values)
{
    unsigned result = 1;
    for (const auto& elem : values)
    {
        result *= elem.size();
    }
    return result;
}

// PatternInputMapping ////////////////////////////////////////////////////////

PatternInputMapping::PatternInputMapping(
    const std::map<unsigned, unsigned>& valuesMapping) :
    unspecified(false),
    valuesMapping(valuesMapping), indexToEntry()
{
    for (const auto& [key, value] : valuesMapping)
    {
        indexToEntry.push_back(key);
    }
}

int PatternInputMapping::operator[](int i) const
{
    return unspecified ? PatternIndex::unspecified : indexToEntry[i];
}

std::size_t PatternInputMapping::size() const
{
    return unspecified ? 1 : indexToEntry.size();
}

bool PatternInputMapping::operator==(const PatternInputMapping& other) const
{
    return this == &other || (this->unspecified && other.unspecified) ||
           this->valuesMapping == other.valuesMapping;
}

unsigned PatternInputMapping::eval(unsigned x) const
{
    if (!unspecified)
    {
        return valuesMapping.at(x);
    }
    else
    {
        std::stringstream ss;
        ss << "The mapping is unspecified "
           << "and cannot be evaluate at point '" << x << "'";
        throw std::runtime_error(ss.str().c_str());
    }
}

std::map<unsigned, unsigned>
    mappingsSum(const std::vector<std::map<unsigned, unsigned>>& maps)
{
    std::map<unsigned, unsigned> result;
    for (unsigned i = 0; i < maps.size(); ++i)
    {
        const auto& map = maps.at(i);
        for (const auto& [key, value] : map)
        {
            if (!result.contains(key))
            {
                result[key] = value;
            }
            else if (value != result.at(key))
            {
                std::stringstream ss;
                ss << "Key mapping conflict: '" << key << "' -> '" << value
                   << "' in map at position " << i << " vs '" << key << "' -> '"
                   << result.at(key) << "' in one of previous maps";
                throw std::runtime_error(ss.str().c_str());
            }
        }
    }
    return result;
}

// DeviceIdPattern ////////////////////////////////////////////////////////////

DeviceIdPattern::DeviceIdPattern(const std::string& devIdPattern) :
    _rawPattern(devIdPattern)
{
    std::vector<std::string> bracketContents;
    DeviceIdPattern::separateTextAndBracketContents(
        _rawPattern, _patternNonBracketFragments, bracketContents);
    // Parse the brackets content
    std::vector<syntax::IndexedBracketMap> indexedBracketMappins;
    for (const auto& bracketContent : bracketContents)
    {
        indexedBracketMappins.push_back(
            syntax::IndexedBracketMap::parse(bracketContent));
    }
    syntax::IndexedBracketMap::fillImplicitInputPositions(
        indexedBracketMappins);
    // marcinw:TODO: calculate input mappings directly from IndexedBracketMap
    // objects
    for (const auto& ibm : indexedBracketMappins)
    {
        _bracketPosToInputPos.push_back(ibm.getInputPosition());
    }
    std::vector<std::map<unsigned, unsigned>> bracketMappings;
    for (const auto& ibm : indexedBracketMappins)
    {
        bracketMappings.push_back(ibm.map());
    }
    _inputMappings = calcInputMappings(
        calcInputIndexToBracketPoss(_bracketPosToInputPos), bracketMappings);
}

std::string DeviceIdPattern::eval(const PatternIndex& pi) const
{
    checkIsInDomain(pi);
    auto evalBracket = [this, &pi](auto inputPos) {
        return _inputMappings[inputPos].eval(pi[inputPos]);
    };
    auto values = _bracketPosToInputPos | std::views::transform(evalBracket);
    std::stringstream ss;
    ss << _patternNonBracketFragments[0];
    for (unsigned i = 0; i < _bracketPosToInputPos.size(); ++i)
    {
        ss << values[i];
        ss << _patternNonBracketFragments[i + 1];
    }
    return ss.str();
}

CartesianProductRange DeviceIdPattern::domain() const
{
    return CartesianProductRange(_inputMappings);
}

std::vector<PatternIndex> DeviceIdPattern::domainVec() const
{
    auto d = domain();
    return std::vector<PatternIndex>(d.begin(), d.end());
}

std::vector<std::string> DeviceIdPattern::values() const
{
    // marcinw:TODO: change implementation to return a range over values
    std::vector<std::string> result;
    for (const auto& x : domain())
    {
        result.push_back(eval(x));
    }
    return result;
}

std::vector<std::string> DeviceIdPattern::valuesVec() const
{
    auto v = values();
    return std::vector<std::string>(v.begin(), v.end());
}

bool DeviceIdPattern::matches(const std::string& str) const
{
    // marcinw:TODO: more efficient implementation
    return !match(str).empty();
    // auto values = this->values();
    // return !(std::find(values.cbegin(), values.cend(), str) ==
    //          values.cend());
}

std::vector<PatternIndex> DeviceIdPattern::match(const std::string& str) const
{
    // marcinw:TODO: more efficient implementation
    std::vector<PatternIndex> res;
    for (const auto& arg : this->domain())
    {
        if (this->eval(arg) == str)
        {
            res.push_back(arg);
        }
    }
    return res;
}

bool DeviceIdPattern::isInjective() const
{
    // marcinw:TODO: more efficient implementation
    auto d = domain();
    auto v = values();
    return std::set<PatternIndex>(d.cbegin(), d.cend()).size() ==
           std::set<std::string>(v.cbegin(), v.cend()).size();
}

void DeviceIdPattern::checkIsInDomain(const PatternIndex& pi) const
{
    auto badPositions = std::views::iota(0u, dim()) |
                        std::views::filter([this, &pi](unsigned i) {
                            return !_inputMappings[i].contains(pi[i]);
                        });
    if (!std::ranges::empty(badPositions))
    {
        std::stringstream ss;
        ss << "Given pattern index tuple '" << pi
           << "' contains indexes outside of pattern's domain at axes: ";
        std::copy(badPositions.begin(), badPositions.end(),
                  std::ostream_iterator<unsigned>(ss, ", "));
        ss << "(values ";
        for (const auto& pos : badPositions)
        {
            PatternIndex::printValueTo(pi[pos], ss);
            ss << ", ";
        }
        ss << ")";
        throw std::runtime_error(ss.str().c_str());
    }
}

std::vector<std::vector<unsigned>> DeviceIdPattern::calcInputIndexToBracketPoss(
    const std::vector<unsigned>& bracketPosToInputPos)
{
    if (bracketPosToInputPos.size() > 0)
    {
        std::vector<std::vector<unsigned>> result(
            *std::ranges::max_element(bracketPosToInputPos.cbegin(),
                                      bracketPosToInputPos.cend()) +
            1);
        for (unsigned i = 0; i < bracketPosToInputPos.size(); ++i)
        {
            result[bracketPosToInputPos[i]].push_back(i);
        }
        return result;
    }
    else // ! bracketPosToInputPos
    {
        return std::vector<std::vector<unsigned>>();
    }
}

PatternInputMapping DeviceIdPattern::calcInputMapping(
    unsigned inputPos, const std::vector<unsigned>& bracketPositions,
    const std::vector<std::map<unsigned, unsigned>>& bracketMappings)
{
    if (bracketPositions.empty())
    {
        return PatternInputMapping();
    }
    else // ! bracketPositions.empty()
    {
        std::map<unsigned, unsigned> resultMap;
        for (unsigned i = 0; i < bracketPositions.size(); ++i)
        {
            const auto& bracketPos = bracketPositions[i];
            for (const auto& [key, value] : bracketMappings[bracketPos])
            {
                if (resultMap.contains(key))
                {
                    if (resultMap.at(key) != value)
                    {
                        std::stringstream ss;
                        ss << "Ambivalent mapping: bracket at position "
                           << bracketPos << " maps " << key << " to " << value
                           << ", but previous brackets at positions ";
                        std::copy_n(bracketPositions.begin(), i,
                                    std::ostream_iterator<unsigned>(ss, ", "));
                        ss << "bound to the same input position " << inputPos
                           << " map " << key << " to " << resultMap.at(key);
                        throw std::runtime_error(ss.str().c_str());
                    }
                }
                else // ! resultMap.contains(key)
                {
                    // marcinw:TODO: awful code, get rid of this
                    auto notDefinedPos = std::find_if_not(
                        bracketPositions.cbegin(), bracketPositions.cend(),
                        [key, &bracketMappings](unsigned i) {
                            return bracketMappings[i].contains(key);
                        });
                    if (notDefinedPos == bracketPositions.cend())
                    {
                        resultMap[key] = value;
                    }
                    else
                    {
                        shortlogs_wrn(
                            << "Incomplete mapping: bracket at position "
                            << bracketPos << " maps " << key << " to " << value
                            << ", but bracket at position " << *notDefinedPos
                            << " bound to the same input position " << inputPos
                            << " doesn't specify any mapping for " << key
                            << ". Dropping from the input domain.");
                    }
                }
            }
        }
        return PatternInputMapping(resultMap);
    }
}

std::vector<PatternInputMapping> DeviceIdPattern::calcInputMappings(
    const std::vector<std::vector<unsigned>>& inputPossToBracketPoss,
    const std::vector<std::map<unsigned, unsigned>>& bracketMappings)
{
    std::vector<PatternInputMapping> result(inputPossToBracketPoss.size());
    for (unsigned inputPosition = 0;
         inputPosition < inputPossToBracketPoss.size(); ++inputPosition)
    {
        result[inputPosition] = calcInputMapping(
            inputPosition, inputPossToBracketPoss[inputPosition],
            bracketMappings);
    }
    return result;
}

std::string DeviceIdPattern::pattern() const
{
    return _rawPattern;
}

std::vector<unsigned> DeviceIdPattern::dimDomain(unsigned axis) const
{
    if (axis < dim())
    {
        std::vector<unsigned> result;
        // marcinw:TODO: terrible, terrible implementation. No time though
        for (const auto& arg : domain())
        {
            if (std::ranges::find(result, (unsigned)arg[axis]) == result.end())
            {
                result.push_back(arg[axis]);
            }
        }
        std::ranges::sort(result);
        return result;
    }
    else // ! axis >= dim()
    {
        return {};
    }
}

} // namespace device_id
