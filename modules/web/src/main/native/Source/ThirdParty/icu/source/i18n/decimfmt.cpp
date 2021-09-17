// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include <cmath>
#include <cstdlib>
#include <stdlib.h>
#include "unicode/errorcode.h"
#include "unicode/decimfmt.h"
#include "number_decimalquantity.h"
#include "number_types.h"
#include "numparse_impl.h"
#include "number_mapper.h"
#include "number_patternstring.h"
#include "putilimp.h"
#include "number_utils.h"
#include "number_utypes.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;
using namespace icu::numparse;
using namespace icu::numparse::impl;
using ERoundingMode = icu::DecimalFormat::ERoundingMode;
using EPadPosition = icu::DecimalFormat::EPadPosition;

// MSVC warns C4805 when comparing bool with UBool
// TODO: Move this macro into a better place?
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN
#define UBOOL_TO_BOOL(b) static_cast<bool>(b)
#else
#define UBOOL_TO_BOOL(b) b
#endif


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DecimalFormat)


DecimalFormat::DecimalFormat(UErrorCode& status)
        : DecimalFormat(nullptr, status) {
    // Use the default locale and decimal pattern.
    const char* localeName = Locale::getDefault().getName();
    LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(status));
    UnicodeString patternString = utils::getPatternForStyle(
            localeName,
            ns->getName(),
            CLDR_PATTERN_STYLE_DECIMAL,
            status);
    setPropertiesFromPattern(patternString, IGNORE_ROUNDING_IF_CURRENCY, status);
    touch(status);
}

DecimalFormat::DecimalFormat(const UnicodeString& pattern, UErrorCode& status)
        : DecimalFormat(nullptr, status) {
    setPropertiesFromPattern(pattern, IGNORE_ROUNDING_IF_CURRENCY, status);
    touch(status);
}

DecimalFormat::DecimalFormat(const UnicodeString& pattern, DecimalFormatSymbols* symbolsToAdopt,
                             UErrorCode& status)
        : DecimalFormat(symbolsToAdopt, status) {
    setPropertiesFromPattern(pattern, IGNORE_ROUNDING_IF_CURRENCY, status);
    touch(status);
}

DecimalFormat::DecimalFormat(const UnicodeString& pattern, DecimalFormatSymbols* symbolsToAdopt,
                             UNumberFormatStyle style, UErrorCode& status)
        : DecimalFormat(symbolsToAdopt, status) {
    // If choice is a currency type, ignore the rounding information.
    if (style == UNumberFormatStyle::UNUM_CURRENCY || style == UNumberFormatStyle::UNUM_CURRENCY_ISO ||
        style == UNumberFormatStyle::UNUM_CURRENCY_ACCOUNTING ||
        style == UNumberFormatStyle::UNUM_CASH_CURRENCY ||
        style == UNumberFormatStyle::UNUM_CURRENCY_STANDARD ||
        style == UNumberFormatStyle::UNUM_CURRENCY_PLURAL) {
        setPropertiesFromPattern(pattern, IGNORE_ROUNDING_ALWAYS, status);
    } else {
        setPropertiesFromPattern(pattern, IGNORE_ROUNDING_IF_CURRENCY, status);
    }
    // Note: in Java, CurrencyPluralInfo is set in NumberFormat.java, but in C++, it is not set there,
    // so we have to set it here.
    if (style == UNumberFormatStyle::UNUM_CURRENCY_PLURAL) {
        LocalPointer<CurrencyPluralInfo> cpi(
                new CurrencyPluralInfo(fields->symbols->getLocale(), status),
                status);
        if (U_FAILURE(status)) { return; }
        fields->properties->currencyPluralInfo.fPtr.adoptInstead(cpi.orphan());
    }
    touch(status);
}

DecimalFormat::DecimalFormat(const DecimalFormatSymbols* symbolsToAdopt, UErrorCode& status) {
    LocalPointer<const DecimalFormatSymbols> adoptedSymbols(symbolsToAdopt);
    fields = new DecimalFormatFields();
    if (U_FAILURE(status)) {
        return;
    }
    if (fields == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    fields->properties.adoptInsteadAndCheckErrorCode(new DecimalFormatProperties(), status);
    fields->exportedProperties.adoptInsteadAndCheckErrorCode(new DecimalFormatProperties(), status);
    if (adoptedSymbols.isNull()) {
        fields->symbols.adoptInsteadAndCheckErrorCode(new DecimalFormatSymbols(status), status);
    } else {
        fields->symbols.adoptInsteadAndCheckErrorCode(adoptedSymbols.orphan(), status);
    }
}

#if UCONFIG_HAVE_PARSEALLINPUT

void DecimalFormat::setParseAllInput(UNumberFormatAttributeValue value) {
    if (value == fields->properties->parseAllInput) { return; }
    fields->properties->parseAllInput = value;
}

#endif

DecimalFormat&
DecimalFormat::setAttribute(UNumberFormatAttribute attr, int32_t newValue, UErrorCode& status) {
    if (U_FAILURE(status)) { return *this; }

    switch (attr) {
        case UNUM_LENIENT_PARSE:
            setLenient(newValue != 0);
            break;

        case UNUM_PARSE_INT_ONLY:
            setParseIntegerOnly(newValue != 0);
            break;

        case UNUM_GROUPING_USED:
            setGroupingUsed(newValue != 0);
            break;

        case UNUM_DECIMAL_ALWAYS_SHOWN:
            setDecimalSeparatorAlwaysShown(newValue != 0);
            break;

        case UNUM_MAX_INTEGER_DIGITS:
            setMaximumIntegerDigits(newValue);
            break;

        case UNUM_MIN_INTEGER_DIGITS:
            setMinimumIntegerDigits(newValue);
            break;

        case UNUM_INTEGER_DIGITS:
            setMinimumIntegerDigits(newValue);
            setMaximumIntegerDigits(newValue);
            break;

        case UNUM_MAX_FRACTION_DIGITS:
            setMaximumFractionDigits(newValue);
            break;

        case UNUM_MIN_FRACTION_DIGITS:
            setMinimumFractionDigits(newValue);
            break;

        case UNUM_FRACTION_DIGITS:
            setMinimumFractionDigits(newValue);
            setMaximumFractionDigits(newValue);
            break;

        case UNUM_SIGNIFICANT_DIGITS_USED:
            setSignificantDigitsUsed(newValue != 0);
            break;

        case UNUM_MAX_SIGNIFICANT_DIGITS:
            setMaximumSignificantDigits(newValue);
            break;

        case UNUM_MIN_SIGNIFICANT_DIGITS:
            setMinimumSignificantDigits(newValue);
            break;

        case UNUM_MULTIPLIER:
            setMultiplier(newValue);
            break;

        case UNUM_SCALE:
            setMultiplierScale(newValue);
            break;

        case UNUM_GROUPING_SIZE:
            setGroupingSize(newValue);
            break;

        case UNUM_ROUNDING_MODE:
            setRoundingMode((DecimalFormat::ERoundingMode) newValue);
            break;

        case UNUM_FORMAT_WIDTH:
            setFormatWidth(newValue);
            break;

        case UNUM_PADDING_POSITION:
            /** The position at which padding will take place. */
            setPadPosition((DecimalFormat::EPadPosition) newValue);
            break;

        case UNUM_SECONDARY_GROUPING_SIZE:
            setSecondaryGroupingSize(newValue);
            break;

#if UCONFIG_HAVE_PARSEALLINPUT
        case UNUM_PARSE_ALL_INPUT:
            setParseAllInput((UNumberFormatAttributeValue) newValue);
            break;
#endif

        case UNUM_PARSE_NO_EXPONENT:
            setParseNoExponent((UBool) newValue);
            break;

        case UNUM_PARSE_DECIMAL_MARK_REQUIRED:
            setDecimalPatternMatchRequired((UBool) newValue);
            break;

        case UNUM_CURRENCY_USAGE:
            setCurrencyUsage((UCurrencyUsage) newValue, &status);
            break;

        case UNUM_MINIMUM_GROUPING_DIGITS:
            setMinimumGroupingDigits(newValue);
            break;

        case UNUM_PARSE_CASE_SENSITIVE:
            setParseCaseSensitive(static_cast<UBool>(newValue));
            break;

        case UNUM_SIGN_ALWAYS_SHOWN:
            setSignAlwaysShown(static_cast<UBool>(newValue));
            break;

        case UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS:
            setFormatFailIfMoreThanMaxDigits(static_cast<UBool>(newValue));
            break;

        default:
            status = U_UNSUPPORTED_ERROR;
            break;
    }
    return *this;
}

int32_t DecimalFormat::getAttribute(UNumberFormatAttribute attr, UErrorCode& status) const {
    if (U_FAILURE(status)) { return -1; }
    switch (attr) {
        case UNUM_LENIENT_PARSE:
            return isLenient();

        case UNUM_PARSE_INT_ONLY:
            return isParseIntegerOnly();

        case UNUM_GROUPING_USED:
            return isGroupingUsed();

        case UNUM_DECIMAL_ALWAYS_SHOWN:
            return isDecimalSeparatorAlwaysShown();

        case UNUM_MAX_INTEGER_DIGITS:
            return getMaximumIntegerDigits();

        case UNUM_MIN_INTEGER_DIGITS:
            return getMinimumIntegerDigits();

        case UNUM_INTEGER_DIGITS:
            // TBD: what should this return?
            return getMinimumIntegerDigits();

        case UNUM_MAX_FRACTION_DIGITS:
            return getMaximumFractionDigits();

        case UNUM_MIN_FRACTION_DIGITS:
            return getMinimumFractionDigits();

        case UNUM_FRACTION_DIGITS:
            // TBD: what should this return?
            return getMinimumFractionDigits();

        case UNUM_SIGNIFICANT_DIGITS_USED:
            return areSignificantDigitsUsed();

        case UNUM_MAX_SIGNIFICANT_DIGITS:
            return getMaximumSignificantDigits();

        case UNUM_MIN_SIGNIFICANT_DIGITS:
            return getMinimumSignificantDigits();

        case UNUM_MULTIPLIER:
            return getMultiplier();

        case UNUM_SCALE:
            return getMultiplierScale();

        case UNUM_GROUPING_SIZE:
            return getGroupingSize();

        case UNUM_ROUNDING_MODE:
            return getRoundingMode();

        case UNUM_FORMAT_WIDTH:
            return getFormatWidth();

        case UNUM_PADDING_POSITION:
            return getPadPosition();

        case UNUM_SECONDARY_GROUPING_SIZE:
            return getSecondaryGroupingSize();

        case UNUM_PARSE_NO_EXPONENT:
            return isParseNoExponent();

        case UNUM_PARSE_DECIMAL_MARK_REQUIRED:
            return isDecimalPatternMatchRequired();

        case UNUM_CURRENCY_USAGE:
            return getCurrencyUsage();

        case UNUM_MINIMUM_GROUPING_DIGITS:
            return getMinimumGroupingDigits();

        case UNUM_PARSE_CASE_SENSITIVE:
            return isParseCaseSensitive();

        case UNUM_SIGN_ALWAYS_SHOWN:
            return isSignAlwaysShown();

        case UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS:
            return isFormatFailIfMoreThanMaxDigits();

        default:
            status = U_UNSUPPORTED_ERROR;
            break;
    }

    return -1; /* undefined */
}

void DecimalFormat::setGroupingUsed(UBool enabled) {
    if (UBOOL_TO_BOOL(enabled) == fields->properties->groupingUsed) { return; }
    NumberFormat::setGroupingUsed(enabled); // to set field for compatibility
    fields->properties->groupingUsed = enabled;
    touchNoError();
}

void DecimalFormat::setParseIntegerOnly(UBool value) {
    if (UBOOL_TO_BOOL(value) == fields->properties->parseIntegerOnly) { return; }
    NumberFormat::setParseIntegerOnly(value); // to set field for compatibility
    fields->properties->parseIntegerOnly = value;
    touchNoError();
}

void DecimalFormat::setLenient(UBool enable) {
    ParseMode mode = enable ? PARSE_MODE_LENIENT : PARSE_MODE_STRICT;
    if (!fields->properties->parseMode.isNull() && mode == fields->properties->parseMode.getNoError()) { return; }
    NumberFormat::setLenient(enable); // to set field for compatibility
    fields->properties->parseMode = mode;
    touchNoError();
}

DecimalFormat::DecimalFormat(const UnicodeString& pattern, DecimalFormatSymbols* symbolsToAdopt,
                             UParseError&, UErrorCode& status)
        : DecimalFormat(symbolsToAdopt, status) {
    // TODO: What is parseError for?
    setPropertiesFromPattern(pattern, IGNORE_ROUNDING_IF_CURRENCY, status);
    touch(status);
}

DecimalFormat::DecimalFormat(const UnicodeString& pattern, const DecimalFormatSymbols& symbols,
                             UErrorCode& status)
        : DecimalFormat(new DecimalFormatSymbols(symbols), status) {
    setPropertiesFromPattern(pattern, IGNORE_ROUNDING_IF_CURRENCY, status);
    touch(status);
}

DecimalFormat::DecimalFormat(const DecimalFormat& source) : NumberFormat(source) {
    // Note: it is not safe to copy fields->formatter or fWarehouse directly because fields->formatter might have
    // dangling pointers to fields inside fWarehouse. The safe thing is to re-construct fields->formatter from
    // the property bag, despite being somewhat slower.
    fields = new DecimalFormatFields();
    if (fields == nullptr) {
        return;
    }
    fields->properties.adoptInstead(new DecimalFormatProperties(*source.fields->properties));
    fields->symbols.adoptInstead(new DecimalFormatSymbols(*source.fields->symbols));
    fields->exportedProperties.adoptInstead(new DecimalFormatProperties());
    if (fields->properties == nullptr || fields->symbols == nullptr || fields->exportedProperties == nullptr) {
        return;
    }
    touchNoError();
}

DecimalFormat& DecimalFormat::operator=(const DecimalFormat& rhs) {
    *fields->properties = *rhs.fields->properties;
    fields->exportedProperties->clear();
    fields->symbols.adoptInstead(new DecimalFormatSymbols(*rhs.fields->symbols));
    touchNoError();
    return *this;
}

DecimalFormat::~DecimalFormat() {
    delete fields->atomicParser.exchange(nullptr);
    delete fields->atomicCurrencyParser.exchange(nullptr);
    delete fields;
}

Format* DecimalFormat::clone() const {
    return new DecimalFormat(*this);
}

UBool DecimalFormat::operator==(const Format& other) const {
    auto* otherDF = dynamic_cast<const DecimalFormat*>(&other);
    if (otherDF == nullptr) {
        return false;
    }
    return *fields->properties == *otherDF->fields->properties && *fields->symbols == *otherDF->fields->symbols;
}

UnicodeString& DecimalFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos) const {
    if (pos.getField() == FieldPosition::DONT_CARE && fastFormatDouble(number, appendTo)) {
        return appendTo;
    }
    UErrorCode localStatus = U_ZERO_ERROR;
    FormattedNumber output = fields->formatter->formatDouble(number, localStatus);
    fieldPositionHelper(output, pos, appendTo.length(), localStatus);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString& DecimalFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos,
                                     UErrorCode& status) const {
    if (pos.getField() == FieldPosition::DONT_CARE && fastFormatDouble(number, appendTo)) {
        return appendTo;
    }
    FormattedNumber output = fields->formatter->formatDouble(number, status);
    fieldPositionHelper(output, pos, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString&
DecimalFormat::format(double number, UnicodeString& appendTo, FieldPositionIterator* posIter,
                      UErrorCode& status) const {
    if (posIter == nullptr && fastFormatDouble(number, appendTo)) {
        return appendTo;
    }
    FormattedNumber output = fields->formatter->formatDouble(number, status);
    fieldPositionIteratorHelper(output, posIter, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString& DecimalFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos) const {
    return format(static_cast<int64_t> (number), appendTo, pos);
}

UnicodeString& DecimalFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos,
                                     UErrorCode& status) const {
    return format(static_cast<int64_t> (number), appendTo, pos, status);
}

UnicodeString&
DecimalFormat::format(int32_t number, UnicodeString& appendTo, FieldPositionIterator* posIter,
                      UErrorCode& status) const {
    return format(static_cast<int64_t> (number), appendTo, posIter, status);
}

UnicodeString& DecimalFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos) const {
    if (pos.getField() == FieldPosition::DONT_CARE && fastFormatInt64(number, appendTo)) {
        return appendTo;
    }
    UErrorCode localStatus = U_ZERO_ERROR;
    FormattedNumber output = fields->formatter->formatInt(number, localStatus);
    fieldPositionHelper(output, pos, appendTo.length(), localStatus);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString& DecimalFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos,
                                     UErrorCode& status) const {
    if (pos.getField() == FieldPosition::DONT_CARE && fastFormatInt64(number, appendTo)) {
        return appendTo;
    }
    FormattedNumber output = fields->formatter->formatInt(number, status);
    fieldPositionHelper(output, pos, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString&
DecimalFormat::format(int64_t number, UnicodeString& appendTo, FieldPositionIterator* posIter,
                      UErrorCode& status) const {
    if (posIter == nullptr && fastFormatInt64(number, appendTo)) {
        return appendTo;
    }
    FormattedNumber output = fields->formatter->formatInt(number, status);
    fieldPositionIteratorHelper(output, posIter, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString&
DecimalFormat::format(StringPiece number, UnicodeString& appendTo, FieldPositionIterator* posIter,
                      UErrorCode& status) const {
    FormattedNumber output = fields->formatter->formatDecimal(number, status);
    fieldPositionIteratorHelper(output, posIter, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString& DecimalFormat::format(const DecimalQuantity& number, UnicodeString& appendTo,
                                     FieldPositionIterator* posIter, UErrorCode& status) const {
    FormattedNumber output = fields->formatter->formatDecimalQuantity(number, status);
    fieldPositionIteratorHelper(output, posIter, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

UnicodeString&
DecimalFormat::format(const DecimalQuantity& number, UnicodeString& appendTo, FieldPosition& pos,
                      UErrorCode& status) const {
    FormattedNumber output = fields->formatter->formatDecimalQuantity(number, status);
    fieldPositionHelper(output, pos, appendTo.length(), status);
    auto appendable = UnicodeStringAppendable(appendTo);
    output.appendTo(appendable);
    return appendTo;
}

void DecimalFormat::parse(const UnicodeString& text, Formattable& output,
                          ParsePosition& parsePosition) const {
    if (parsePosition.getIndex() < 0 || parsePosition.getIndex() >= text.length()) {
        return;
    }

    ErrorCode status;
    ParsedNumber result;
    // Note: if this is a currency instance, currencies will be matched despite the fact that we are not in the
    // parseCurrency method (backwards compatibility)
    int32_t startIndex = parsePosition.getIndex();
    const NumberParserImpl* parser = getParser(status);
    if (U_FAILURE(status)) { return; }
    parser->parse(text, startIndex, true, result, status);
    // TODO: Do we need to check for fImpl->properties->parseAllInput (UCONFIG_HAVE_PARSEALLINPUT) here?
    if (result.success()) {
        parsePosition.setIndex(result.charEnd);
        result.populateFormattable(output, parser->getParseFlags());
    } else {
        parsePosition.setErrorIndex(startIndex + result.charEnd);
    }
}

CurrencyAmount* DecimalFormat::parseCurrency(const UnicodeString& text, ParsePosition& parsePosition) const {
    if (parsePosition.getIndex() < 0 || parsePosition.getIndex() >= text.length()) {
        return nullptr;
    }

    ErrorCode status;
    ParsedNumber result;
    // Note: if this is a currency instance, currencies will be matched despite the fact that we are not in the
    // parseCurrency method (backwards compatibility)
    int32_t startIndex = parsePosition.getIndex();
    const NumberParserImpl* parser = getCurrencyParser(status);
    if (U_FAILURE(status)) { return nullptr; }
    parser->parse(text, startIndex, true, result, status);
    // TODO: Do we need to check for fImpl->properties->parseAllInput (UCONFIG_HAVE_PARSEALLINPUT) here?
    if (result.success()) {
        parsePosition.setIndex(result.charEnd);
        Formattable formattable;
        result.populateFormattable(formattable, parser->getParseFlags());
        return new CurrencyAmount(formattable, result.currencyCode, status);
    } else {
        parsePosition.setErrorIndex(startIndex + result.charEnd);
        return nullptr;
    }
}

const DecimalFormatSymbols* DecimalFormat::getDecimalFormatSymbols(void) const {
    return fields->symbols.getAlias();
}

void DecimalFormat::adoptDecimalFormatSymbols(DecimalFormatSymbols* symbolsToAdopt) {
    if (symbolsToAdopt == nullptr) {
        return; // do not allow caller to set fields->symbols to NULL
    }
    fields->symbols.adoptInstead(symbolsToAdopt);
    touchNoError();
}

void DecimalFormat::setDecimalFormatSymbols(const DecimalFormatSymbols& symbols) {
    fields->symbols.adoptInstead(new DecimalFormatSymbols(symbols));
    touchNoError();
}

const CurrencyPluralInfo* DecimalFormat::getCurrencyPluralInfo(void) const {
    return fields->properties->currencyPluralInfo.fPtr.getAlias();
}

void DecimalFormat::adoptCurrencyPluralInfo(CurrencyPluralInfo* toAdopt) {
    fields->properties->currencyPluralInfo.fPtr.adoptInstead(toAdopt);
    touchNoError();
}

void DecimalFormat::setCurrencyPluralInfo(const CurrencyPluralInfo& info) {
    if (fields->properties->currencyPluralInfo.fPtr.isNull()) {
        fields->properties->currencyPluralInfo.fPtr.adoptInstead(info.clone());
    } else {
        *fields->properties->currencyPluralInfo.fPtr = info; // copy-assignment operator
    }
    touchNoError();
}

UnicodeString& DecimalFormat::getPositivePrefix(UnicodeString& result) const {
    ErrorCode localStatus;
    fields->formatter->getAffixImpl(true, false, result, localStatus);
    return result;
}

void DecimalFormat::setPositivePrefix(const UnicodeString& newValue) {
    if (newValue == fields->properties->positivePrefix) { return; }
    fields->properties->positivePrefix = newValue;
    touchNoError();
}

UnicodeString& DecimalFormat::getNegativePrefix(UnicodeString& result) const {
    ErrorCode localStatus;
    fields->formatter->getAffixImpl(true, true, result, localStatus);
    return result;
}

void DecimalFormat::setNegativePrefix(const UnicodeString& newValue) {
    if (newValue == fields->properties->negativePrefix) { return; }
    fields->properties->negativePrefix = newValue;
    touchNoError();
}

UnicodeString& DecimalFormat::getPositiveSuffix(UnicodeString& result) const {
    ErrorCode localStatus;
    fields->formatter->getAffixImpl(false, false, result, localStatus);
    return result;
}

void DecimalFormat::setPositiveSuffix(const UnicodeString& newValue) {
    if (newValue == fields->properties->positiveSuffix) { return; }
    fields->properties->positiveSuffix = newValue;
    touchNoError();
}

UnicodeString& DecimalFormat::getNegativeSuffix(UnicodeString& result) const {
    ErrorCode localStatus;
    fields->formatter->getAffixImpl(false, true, result, localStatus);
    return result;
}

void DecimalFormat::setNegativeSuffix(const UnicodeString& newValue) {
    if (newValue == fields->properties->negativeSuffix) { return; }
    fields->properties->negativeSuffix = newValue;
    touchNoError();
}

UBool DecimalFormat::isSignAlwaysShown() const {
    return fields->properties->signAlwaysShown;
}

void DecimalFormat::setSignAlwaysShown(UBool value) {
    if (UBOOL_TO_BOOL(value) == fields->properties->signAlwaysShown) { return; }
    fields->properties->signAlwaysShown = value;
    touchNoError();
}

int32_t DecimalFormat::getMultiplier(void) const {
    if (fields->properties->multiplier != 1) {
        return fields->properties->multiplier;
    } else if (fields->properties->magnitudeMultiplier != 0) {
        return static_cast<int32_t>(uprv_pow10(fields->properties->magnitudeMultiplier));
    } else {
        return 1;
    }
}

void DecimalFormat::setMultiplier(int32_t multiplier) {
    if (multiplier == 0) {
        multiplier = 1;     // one being the benign default value for a multiplier.
    }

    // Try to convert to a magnitude multiplier first
    int delta = 0;
    int value = multiplier;
    while (value != 1) {
        delta++;
        int temp = value / 10;
        if (temp * 10 != value) {
            delta = -1;
            break;
        }
        value = temp;
    }
    if (delta != -1) {
        fields->properties->magnitudeMultiplier = delta;
        fields->properties->multiplier = 1;
    } else {
        fields->properties->magnitudeMultiplier = 0;
        fields->properties->multiplier = multiplier;
    }
    touchNoError();
}

int32_t DecimalFormat::getMultiplierScale() const {
    return fields->properties->multiplierScale;
}

void DecimalFormat::setMultiplierScale(int32_t newValue) {
    if (newValue == fields->properties->multiplierScale) { return; }
    fields->properties->multiplierScale = newValue;
    touchNoError();
}

double DecimalFormat::getRoundingIncrement(void) const {
    return fields->exportedProperties->roundingIncrement;
}

void DecimalFormat::setRoundingIncrement(double newValue) {
    if (newValue == fields->properties->roundingIncrement) { return; }
    fields->properties->roundingIncrement = newValue;
    touchNoError();
}

ERoundingMode DecimalFormat::getRoundingMode(void) const {
    // UNumberFormatRoundingMode and ERoundingMode have the same values.
    return static_cast<ERoundingMode>(fields->exportedProperties->roundingMode.getNoError());
}

void DecimalFormat::setRoundingMode(ERoundingMode roundingMode) {
    auto uRoundingMode = static_cast<UNumberFormatRoundingMode>(roundingMode);
    if (!fields->properties->roundingMode.isNull() && uRoundingMode == fields->properties->roundingMode.getNoError()) {
        return;
    }
    NumberFormat::setMaximumIntegerDigits(roundingMode); // to set field for compatibility
    fields->properties->roundingMode = uRoundingMode;
    touchNoError();
}

int32_t DecimalFormat::getFormatWidth(void) const {
    return fields->properties->formatWidth;
}

void DecimalFormat::setFormatWidth(int32_t width) {
    if (width == fields->properties->formatWidth) { return; }
    fields->properties->formatWidth = width;
    touchNoError();
}

UnicodeString DecimalFormat::getPadCharacterString() const {
    if (fields->properties->padString.isBogus()) {
        // Readonly-alias the static string kFallbackPaddingString
        return {TRUE, kFallbackPaddingString, -1};
    } else {
        return fields->properties->padString;
    }
}

void DecimalFormat::setPadCharacter(const UnicodeString& padChar) {
    if (padChar == fields->properties->padString) { return; }
    if (padChar.length() > 0) {
        fields->properties->padString = UnicodeString(padChar.char32At(0));
    } else {
        fields->properties->padString.setToBogus();
    }
    touchNoError();
}

EPadPosition DecimalFormat::getPadPosition(void) const {
    if (fields->properties->padPosition.isNull()) {
        return EPadPosition::kPadBeforePrefix;
    } else {
        // UNumberFormatPadPosition and EPadPosition have the same values.
        return static_cast<EPadPosition>(fields->properties->padPosition.getNoError());
    }
}

void DecimalFormat::setPadPosition(EPadPosition padPos) {
    auto uPadPos = static_cast<UNumberFormatPadPosition>(padPos);
    if (!fields->properties->padPosition.isNull() && uPadPos == fields->properties->padPosition.getNoError()) {
        return;
    }
    fields->properties->padPosition = uPadPos;
    touchNoError();
}

UBool DecimalFormat::isScientificNotation(void) const {
    return fields->properties->minimumExponentDigits != -1;
}

void DecimalFormat::setScientificNotation(UBool useScientific) {
    int32_t minExp = useScientific ? 1 : -1;
    if (fields->properties->minimumExponentDigits == minExp) { return; }
    if (useScientific) {
        fields->properties->minimumExponentDigits = 1;
    } else {
        fields->properties->minimumExponentDigits = -1;
    }
    touchNoError();
}

int8_t DecimalFormat::getMinimumExponentDigits(void) const {
    return static_cast<int8_t>(fields->properties->minimumExponentDigits);
}

void DecimalFormat::setMinimumExponentDigits(int8_t minExpDig) {
    if (minExpDig == fields->properties->minimumExponentDigits) { return; }
    fields->properties->minimumExponentDigits = minExpDig;
    touchNoError();
}

UBool DecimalFormat::isExponentSignAlwaysShown(void) const {
    return fields->properties->exponentSignAlwaysShown;
}

void DecimalFormat::setExponentSignAlwaysShown(UBool expSignAlways) {
    if (UBOOL_TO_BOOL(expSignAlways) == fields->properties->exponentSignAlwaysShown) { return; }
    fields->properties->exponentSignAlwaysShown = expSignAlways;
    touchNoError();
}

int32_t DecimalFormat::getGroupingSize(void) const {
    if (fields->properties->groupingSize < 0) {
        return 0;
    }
    return fields->properties->groupingSize;
}

void DecimalFormat::setGroupingSize(int32_t newValue) {
    if (newValue == fields->properties->groupingSize) { return; }
    fields->properties->groupingSize = newValue;
    touchNoError();
}

int32_t DecimalFormat::getSecondaryGroupingSize(void) const {
    int grouping2 = fields->properties->secondaryGroupingSize;
    if (grouping2 < 0) {
        return 0;
    }
    return grouping2;
}

void DecimalFormat::setSecondaryGroupingSize(int32_t newValue) {
    if (newValue == fields->properties->secondaryGroupingSize) { return; }
    fields->properties->secondaryGroupingSize = newValue;
    touchNoError();
}

int32_t DecimalFormat::getMinimumGroupingDigits() const {
    return fields->properties->minimumGroupingDigits;
}

void DecimalFormat::setMinimumGroupingDigits(int32_t newValue) {
    if (newValue == fields->properties->minimumGroupingDigits) { return; }
    fields->properties->minimumGroupingDigits = newValue;
    touchNoError();
}

UBool DecimalFormat::isDecimalSeparatorAlwaysShown(void) const {
    return fields->properties->decimalSeparatorAlwaysShown;
}

void DecimalFormat::setDecimalSeparatorAlwaysShown(UBool newValue) {
    if (UBOOL_TO_BOOL(newValue) == fields->properties->decimalSeparatorAlwaysShown) { return; }
    fields->properties->decimalSeparatorAlwaysShown = newValue;
    touchNoError();
}

UBool DecimalFormat::isDecimalPatternMatchRequired(void) const {
    return fields->properties->decimalPatternMatchRequired;
}

void DecimalFormat::setDecimalPatternMatchRequired(UBool newValue) {
    if (UBOOL_TO_BOOL(newValue) == fields->properties->decimalPatternMatchRequired) { return; }
    fields->properties->decimalPatternMatchRequired = newValue;
    touchNoError();
}

UBool DecimalFormat::isParseNoExponent() const {
    return fields->properties->parseNoExponent;
}

void DecimalFormat::setParseNoExponent(UBool value) {
    if (UBOOL_TO_BOOL(value) == fields->properties->parseNoExponent) { return; }
    fields->properties->parseNoExponent = value;
    touchNoError();
}

UBool DecimalFormat::isParseCaseSensitive() const {
    return fields->properties->parseCaseSensitive;
}

void DecimalFormat::setParseCaseSensitive(UBool value) {
    if (UBOOL_TO_BOOL(value) == fields->properties->parseCaseSensitive) { return; }
    fields->properties->parseCaseSensitive = value;
    touchNoError();
}

UBool DecimalFormat::isFormatFailIfMoreThanMaxDigits() const {
    return fields->properties->formatFailIfMoreThanMaxDigits;
}

void DecimalFormat::setFormatFailIfMoreThanMaxDigits(UBool value) {
    if (UBOOL_TO_BOOL(value) == fields->properties->formatFailIfMoreThanMaxDigits) { return; }
    fields->properties->formatFailIfMoreThanMaxDigits = value;
    touchNoError();
}

UnicodeString& DecimalFormat::toPattern(UnicodeString& result) const {
    // Pull some properties from exportedProperties and others from properties
    // to keep affix patterns intact.  In particular, pull rounding properties
    // so that CurrencyUsage is reflected properly.
    // TODO: Consider putting this logic in number_patternstring.cpp instead.
    ErrorCode localStatus;
    DecimalFormatProperties tprops(*fields->properties);
    bool useCurrency = ((!tprops.currency.isNull()) || !tprops.currencyPluralInfo.fPtr.isNull() ||
                        !tprops.currencyUsage.isNull() || AffixUtils::hasCurrencySymbols(
            tprops.positivePrefixPattern, localStatus) || AffixUtils::hasCurrencySymbols(
            tprops.positiveSuffixPattern, localStatus) || AffixUtils::hasCurrencySymbols(
            tprops.negativePrefixPattern, localStatus) || AffixUtils::hasCurrencySymbols(
            tprops.negativeSuffixPattern, localStatus));
    if (useCurrency) {
        tprops.minimumFractionDigits = fields->exportedProperties->minimumFractionDigits;
        tprops.maximumFractionDigits = fields->exportedProperties->maximumFractionDigits;
        tprops.roundingIncrement = fields->exportedProperties->roundingIncrement;
    }
    result = PatternStringUtils::propertiesToPatternString(tprops, localStatus);
    return result;
}

UnicodeString& DecimalFormat::toLocalizedPattern(UnicodeString& result) const {
    ErrorCode localStatus;
    result = toPattern(result);
    result = PatternStringUtils::convertLocalized(result, *fields->symbols, true, localStatus);
    return result;
}

void DecimalFormat::applyPattern(const UnicodeString& pattern, UParseError&, UErrorCode& status) {
    // TODO: What is parseError for?
    applyPattern(pattern, status);
}

void DecimalFormat::applyPattern(const UnicodeString& pattern, UErrorCode& status) {
    setPropertiesFromPattern(pattern, IGNORE_ROUNDING_NEVER, status);
    touch(status);
}

void DecimalFormat::applyLocalizedPattern(const UnicodeString& localizedPattern, UParseError&,
                                          UErrorCode& status) {
    // TODO: What is parseError for?
    applyLocalizedPattern(localizedPattern, status);
}

void DecimalFormat::applyLocalizedPattern(const UnicodeString& localizedPattern, UErrorCode& status) {
    if (U_SUCCESS(status)) {
        UnicodeString pattern = PatternStringUtils::convertLocalized(
                localizedPattern, *fields->symbols, false, status);
        applyPattern(pattern, status);
    }
}

void DecimalFormat::setMaximumIntegerDigits(int32_t newValue) {
    if (newValue == fields->properties->maximumIntegerDigits) { return; }
    // For backwards compatibility, conflicting min/max need to keep the most recent setting.
    int32_t min = fields->properties->minimumIntegerDigits;
    if (min >= 0 && min > newValue) {
        fields->properties->minimumIntegerDigits = newValue;
    }
    fields->properties->maximumIntegerDigits = newValue;
    touchNoError();
}

void DecimalFormat::setMinimumIntegerDigits(int32_t newValue) {
    if (newValue == fields->properties->minimumIntegerDigits) { return; }
    // For backwards compatibility, conflicting min/max need to keep the most recent setting.
    int32_t max = fields->properties->maximumIntegerDigits;
    if (max >= 0 && max < newValue) {
        fields->properties->maximumIntegerDigits = newValue;
    }
    fields->properties->minimumIntegerDigits = newValue;
    touchNoError();
}

void DecimalFormat::setMaximumFractionDigits(int32_t newValue) {
    if (newValue == fields->properties->maximumFractionDigits) { return; }
    // For backwards compatibility, conflicting min/max need to keep the most recent setting.
    int32_t min = fields->properties->minimumFractionDigits;
    if (min >= 0 && min > newValue) {
        fields->properties->minimumFractionDigits = newValue;
    }
    fields->properties->maximumFractionDigits = newValue;
    touchNoError();
}

void DecimalFormat::setMinimumFractionDigits(int32_t newValue) {
    if (newValue == fields->properties->minimumFractionDigits) { return; }
    // For backwards compatibility, conflicting min/max need to keep the most recent setting.
    int32_t max = fields->properties->maximumFractionDigits;
    if (max >= 0 && max < newValue) {
        fields->properties->maximumFractionDigits = newValue;
    }
    fields->properties->minimumFractionDigits = newValue;
    touchNoError();
}

int32_t DecimalFormat::getMinimumSignificantDigits() const {
    return fields->exportedProperties->minimumSignificantDigits;
}

int32_t DecimalFormat::getMaximumSignificantDigits() const {
    return fields->exportedProperties->maximumSignificantDigits;
}

void DecimalFormat::setMinimumSignificantDigits(int32_t value) {
    if (value == fields->properties->minimumSignificantDigits) { return; }
    int32_t max = fields->properties->maximumSignificantDigits;
    if (max >= 0 && max < value) {
        fields->properties->maximumSignificantDigits = value;
    }
    fields->properties->minimumSignificantDigits = value;
    touchNoError();
}

void DecimalFormat::setMaximumSignificantDigits(int32_t value) {
    if (value == fields->properties->maximumSignificantDigits) { return; }
    int32_t min = fields->properties->minimumSignificantDigits;
    if (min >= 0 && min > value) {
        fields->properties->minimumSignificantDigits = value;
    }
    fields->properties->maximumSignificantDigits = value;
    touchNoError();
}

UBool DecimalFormat::areSignificantDigitsUsed() const {
    return fields->properties->minimumSignificantDigits != -1 || fields->properties->maximumSignificantDigits != -1;
}

void DecimalFormat::setSignificantDigitsUsed(UBool useSignificantDigits) {
    // These are the default values from the old implementation.
    int32_t minSig = useSignificantDigits ? 1 : -1;
    int32_t maxSig = useSignificantDigits ? 6 : -1;
    if (fields->properties->minimumSignificantDigits == minSig &&
        fields->properties->maximumSignificantDigits == maxSig) {
        return;
    }
    fields->properties->minimumSignificantDigits = minSig;
    fields->properties->maximumSignificantDigits = maxSig;
    touchNoError();
}

void DecimalFormat::setCurrency(const char16_t* theCurrency, UErrorCode& ec) {
    CurrencyUnit currencyUnit(theCurrency, ec);
    if (U_FAILURE(ec)) { return; }
    if (!fields->properties->currency.isNull() && fields->properties->currency.getNoError() == currencyUnit) {
        return;
    }
    NumberFormat::setCurrency(theCurrency, ec); // to set field for compatibility
    fields->properties->currency = currencyUnit;
    // TODO: Set values in fields->symbols, too?
    touchNoError();
}

void DecimalFormat::setCurrency(const char16_t* theCurrency) {
    ErrorCode localStatus;
    setCurrency(theCurrency, localStatus);
}

void DecimalFormat::setCurrencyUsage(UCurrencyUsage newUsage, UErrorCode* ec) {
    if (U_FAILURE(*ec)) {
        return;
    }
    if (!fields->properties->currencyUsage.isNull() && newUsage == fields->properties->currencyUsage.getNoError()) {
        return;
    }
    fields->properties->currencyUsage = newUsage;
    touch(*ec);
}

UCurrencyUsage DecimalFormat::getCurrencyUsage() const {
    // CurrencyUsage is not exported, so we have to get it from the input property bag.
    // TODO: Should we export CurrencyUsage instead?
    if (fields->properties->currencyUsage.isNull()) {
        return UCURR_USAGE_STANDARD;
    }
    return fields->properties->currencyUsage.getNoError();
}

void
DecimalFormat::formatToDecimalQuantity(double number, DecimalQuantity& output, UErrorCode& status) const {
    fields->formatter->formatDouble(number, status).getDecimalQuantity(output, status);
}

void DecimalFormat::formatToDecimalQuantity(const Formattable& number, DecimalQuantity& output,
                                            UErrorCode& status) const {
    UFormattedNumberData obj;
    number.populateDecimalQuantity(obj.quantity, status);
    fields->formatter->formatImpl(&obj, status);
    output = std::move(obj.quantity);
}

const number::LocalizedNumberFormatter& DecimalFormat::toNumberFormatter() const {
    return *fields->formatter;
}

/** Rebuilds the formatter object from the property bag. */
void DecimalFormat::touch(UErrorCode& status) {
    if (fields->exportedProperties == nullptr) {
        // fields->exportedProperties is null only when the formatter is not ready yet.
        // The only time when this happens is during legacy deserialization.
        return;
    }

    // In C++, fields->symbols is the source of truth for the locale.
    Locale locale = fields->symbols->getLocale();

    // Note: The formatter is relatively cheap to create, and we need it to populate fields->exportedProperties,
    // so automatically compute it here. The parser is a bit more expensive and is not needed until the
    // parse method is called, so defer that until needed.
    // TODO: Only update the pieces that changed instead of re-computing the whole formatter?
    fields->formatter.adoptInstead(
            new LocalizedNumberFormatter(
                    NumberPropertyMapper::create(
                            *fields->properties, *fields->symbols, fields->warehouse, *fields->exportedProperties, status).locale(
                            locale)));

    // Do this after fields->exportedProperties are set up
    setupFastFormat();

    // Delete the parsers if they were made previously
    delete fields->atomicParser.exchange(nullptr);
    delete fields->atomicCurrencyParser.exchange(nullptr);

    // In order for the getters to work, we need to populate some fields in NumberFormat.
    NumberFormat::setCurrency(fields->exportedProperties->currency.get(status).getISOCurrency(), status);
    NumberFormat::setMaximumIntegerDigits(fields->exportedProperties->maximumIntegerDigits);
    NumberFormat::setMinimumIntegerDigits(fields->exportedProperties->minimumIntegerDigits);
    NumberFormat::setMaximumFractionDigits(fields->exportedProperties->maximumFractionDigits);
    NumberFormat::setMinimumFractionDigits(fields->exportedProperties->minimumFractionDigits);
    // fImpl->properties, not fields->exportedProperties, since this information comes from the pattern:
    NumberFormat::setGroupingUsed(fields->properties->groupingUsed);
}

void DecimalFormat::touchNoError() {
    UErrorCode localStatus = U_ZERO_ERROR;
    touch(localStatus);
}

void DecimalFormat::setPropertiesFromPattern(const UnicodeString& pattern, int32_t ignoreRounding,
                                             UErrorCode& status) {
    if (U_SUCCESS(status)) {
        // Cast workaround to get around putting the enum in the public header file
        auto actualIgnoreRounding = static_cast<IgnoreRounding>(ignoreRounding);
        PatternParser::parseToExistingProperties(pattern, *fields->properties,  actualIgnoreRounding, status);
    }
}

const numparse::impl::NumberParserImpl* DecimalFormat::getParser(UErrorCode& status) const {
    if (U_FAILURE(status)) { return nullptr; }

    // First try to get the pre-computed parser
    auto* ptr = fields->atomicParser.load();
    if (ptr != nullptr) {
        return ptr;
    }

    // Try computing the parser on our own
    auto* temp = NumberParserImpl::createParserFromProperties(*fields->properties, *fields->symbols, false, status);
    if (temp == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        // although we may still dereference, call sites should be guarded
    }

    // Note: ptr starts as nullptr; during compare_exchange, it is set to what is actually stored in the
    // atomic if another thread beat us to computing the parser object.
    auto* nonConstThis = const_cast<DecimalFormat*>(this);
    if (!nonConstThis->fields->atomicParser.compare_exchange_strong(ptr, temp)) {
        // Another thread beat us to computing the parser
        delete temp;
        return ptr;
    } else {
        // Our copy of the parser got stored in the atomic
        return temp;
    }
}

const numparse::impl::NumberParserImpl* DecimalFormat::getCurrencyParser(UErrorCode& status) const {
    if (U_FAILURE(status)) { return nullptr; }

    // First try to get the pre-computed parser
    auto* ptr = fields->atomicCurrencyParser.load();
    if (ptr != nullptr) {
        return ptr;
    }

    // Try computing the parser on our own
    auto* temp = NumberParserImpl::createParserFromProperties(*fields->properties, *fields->symbols, true, status);
    if (temp == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        // although we may still dereference, call sites should be guarded
    }

    // Note: ptr starts as nullptr; during compare_exchange, it is set to what is actually stored in the
    // atomic if another thread beat us to computing the parser object.
    auto* nonConstThis = const_cast<DecimalFormat*>(this);
    if (!nonConstThis->fields->atomicCurrencyParser.compare_exchange_strong(ptr, temp)) {
        // Another thread beat us to computing the parser
        delete temp;
        return ptr;
    } else {
        // Our copy of the parser got stored in the atomic
        return temp;
    }
}

void
DecimalFormat::fieldPositionHelper(const number::FormattedNumber& formatted, FieldPosition& fieldPosition,
                                   int32_t offset, UErrorCode& status) {
    // always return first occurrence:
    fieldPosition.setBeginIndex(0);
    fieldPosition.setEndIndex(0);
    bool found = formatted.nextFieldPosition(fieldPosition, status);
    if (found && offset != 0) {
        FieldPositionOnlyHandler fpoh(fieldPosition);
        fpoh.shiftLast(offset);
    }
}

void
DecimalFormat::fieldPositionIteratorHelper(const number::FormattedNumber& formatted, FieldPositionIterator* fpi,
                                           int32_t offset, UErrorCode& status) {
    if (fpi != nullptr) {
        FieldPositionIteratorHandler fpih(fpi, status);
        fpih.setShift(offset);
        formatted.getAllFieldPositionsImpl(fpih, status);
    }
}

// To debug fast-format, change void(x) to printf(x)
#define trace(x) void(x)

void DecimalFormat::setupFastFormat() {
    // Check the majority of properties:
    if (!fields->properties->equalsDefaultExceptFastFormat()) {
        trace("no fast format: equality\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Now check the remaining properties.
    // Nontrivial affixes:
    UBool trivialPP = fields->properties->positivePrefixPattern.isEmpty();
    UBool trivialPS = fields->properties->positiveSuffixPattern.isEmpty();
    UBool trivialNP = fields->properties->negativePrefixPattern.isBogus() || (
            fields->properties->negativePrefixPattern.length() == 1 &&
            fields->properties->negativePrefixPattern.charAt(0) == u'-');
    UBool trivialNS = fields->properties->negativeSuffixPattern.isEmpty();
    if (!trivialPP || !trivialPS || !trivialNP || !trivialNS) {
        trace("no fast format: affixes\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Grouping (secondary grouping is forbidden in equalsDefaultExceptFastFormat):
    bool groupingUsed = fields->properties->groupingUsed;
    int32_t groupingSize = fields->properties->groupingSize;
    bool unusualGroupingSize = groupingSize > 0 && groupingSize != 3;
    const UnicodeString& groupingString = fields->symbols->getConstSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol);
    if (groupingUsed && (unusualGroupingSize || groupingString.length() != 1)) {
        trace("no fast format: grouping\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Integer length:
    int32_t minInt = fields->exportedProperties->minimumIntegerDigits;
    int32_t maxInt = fields->exportedProperties->maximumIntegerDigits;
    // Fastpath supports up to only 10 digits (length of INT32_MIN)
    if (minInt > 10) {
        trace("no fast format: integer\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Fraction length (no fraction part allowed in fast path):
    int32_t minFrac = fields->exportedProperties->minimumFractionDigits;
    if (minFrac > 0) {
        trace("no fast format: fraction\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Other symbols:
    const UnicodeString& minusSignString = fields->symbols->getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
    UChar32 codePointZero = fields->symbols->getCodePointZero();
    if (minusSignString.length() != 1 || U16_LENGTH(codePointZero) != 1) {
        trace("no fast format: symbols\n");
        fields->canUseFastFormat = false;
        return;
    }

    // Good to go!
    trace("can use fast format!\n");
    fields->canUseFastFormat = true;
    fields->fastData.cpZero = static_cast<char16_t>(codePointZero);
    fields->fastData.cpGroupingSeparator = groupingUsed && groupingSize == 3 ? groupingString.charAt(0) : 0;
    fields->fastData.cpMinusSign = minusSignString.charAt(0);
    fields->fastData.minInt = (minInt < 0 || minInt > 127) ? 0 : static_cast<int8_t>(minInt);
    fields->fastData.maxInt = (maxInt < 0 || maxInt > 127) ? 127 : static_cast<int8_t>(maxInt);
}

bool DecimalFormat::fastFormatDouble(double input, UnicodeString& output) const {
    if (!fields->canUseFastFormat) {
        return false;
    }
    if (std::isnan(input)
            || std::trunc(input) != input
            || input <= INT32_MIN
            || input > INT32_MAX) {
        return false;
    }
    doFastFormatInt32(static_cast<int32_t>(input), std::signbit(input), output);
    return true;
}

bool DecimalFormat::fastFormatInt64(int64_t input, UnicodeString& output) const {
    if (!fields->canUseFastFormat) {
        return false;
    }
    if (input <= INT32_MIN || input > INT32_MAX) {
        return false;
    }
    doFastFormatInt32(static_cast<int32_t>(input), input < 0, output);
    return true;
}

void DecimalFormat::doFastFormatInt32(int32_t input, bool isNegative, UnicodeString& output) const {
    U_ASSERT(fields->canUseFastFormat);
    if (isNegative) {
        output.append(fields->fastData.cpMinusSign);
        U_ASSERT(input != INT32_MIN);  // handled by callers
        input = -input;
    }
    // Cap at int32_t to make the buffer small and operations fast.
    // Longest string: "2,147,483,648" (13 chars in length)
    static constexpr int32_t localCapacity = 13;
    char16_t localBuffer[localCapacity];
    char16_t* ptr = localBuffer + localCapacity;
    int8_t group = 0;
    for (int8_t i = 0; i < fields->fastData.maxInt && (input != 0 || i < fields->fastData.minInt); i++) {
        if (group++ == 3 && fields->fastData.cpGroupingSeparator != 0) {
            *(--ptr) = fields->fastData.cpGroupingSeparator;
            group = 1;
        }
        std::div_t res = std::div(input, 10);
        *(--ptr) = static_cast<char16_t>(fields->fastData.cpZero + res.rem);
        input = res.quot;
    }
    int32_t len = localCapacity - static_cast<int32_t>(ptr - localBuffer);
    output.append(ptr, len);
}


#endif /* #if !UCONFIG_NO_FORMATTING */