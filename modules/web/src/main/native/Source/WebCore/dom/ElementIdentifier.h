/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

//#include <wtf/ObjectIdentifier.h>
#include <atomic>
#include <mutex>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum ElementIdentifierType { };
//using ElementIdentifier = ObjectIdentifier<ElementIdentifierType>;
class ElementIdentifierBase {
protected:
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
    WTF_EXPORT_PRIVATE static uint64_t generateThreadSafeIdentifierInternal();
};

class ElementIdentifier : private ElementIdentifierBase {
public:
    static ElementIdentifier generate()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ElementIdentifier { generateIdentifierInternal() };
    }

    static ElementIdentifier generateThreadSafe()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return ElementIdentifier { generateThreadSafeIdentifierInternal() };
    }

    static void enableGenerationProtection()
    {
        m_generationProtected = true;
    }

    ElementIdentifier() = default;

    ElementIdentifier(WTF::HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        ASSERT(isValidIdentifier(m_identifier));
        encoder << m_identifier;
    }
    template<typename Decoder> static Optional<ElementIdentifier> decode(Decoder& decoder)
    {
        Optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier || !isValidIdentifier(*identifier))
            return WTF::nullopt;
        return ElementIdentifier { *identifier };
    }

    bool operator==(const ElementIdentifier& other) const
    {
        return m_identifier == other.m_identifier;
    }

    bool operator!=(const ElementIdentifier& other) const
    {
        return m_identifier != other.m_identifier;
    }

    operator uint64_t() const { return m_identifier; }
    uint64_t toUInt64() const { return m_identifier; }
    explicit operator bool() const { return m_identifier; }

    String loggingString() const
    {
        return String::number(m_identifier);
    }

    struct MarkableTraits {
        static bool isEmptyValue(ElementIdentifier identifier)
        {
            return !identifier.m_identifier;
        }

        static constexpr ElementIdentifier emptyValue()
        {
            return ElementIdentifier();
        }
    };

private:
    friend ElementIdentifier makeElementIdentifier(uint64_t);
    friend struct HashTraits<ElementIdentifier>;
    friend struct ElementIdentifierHash;

    static uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }
    static bool isValidIdentifier(uint64_t identifier) { return identifier && identifier != hashTableDeletedValue(); }

    explicit constexpr ElementIdentifier(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    uint64_t m_identifier { 0 };
    inline static bool m_generationProtected { false };
};

inline ElementIdentifier makeElementIdentifier(uint64_t identifier)
{
    return ElementIdentifier { identifier };
}

struct ElementIdentifierHash {
    static unsigned hash(const ElementIdentifier& identifier) { return WTF::intHash(identifier.m_identifier); }
    static bool equal(const ElementIdentifier& a, const ElementIdentifier& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

inline TextStream& operator<<(TextStream& ts, const ElementIdentifier& identifier)
{
    ts << identifier.toUInt64();
    return ts;
}

inline uint64_t ElementIdentifierBase::generateIdentifierInternal()
{
    static uint64_t current;
    return ++current;
}

inline uint64_t ElementIdentifierBase::generateThreadSafeIdentifierInternal()
{
    static LazyNeverDestroyed<std::atomic<uint64_t>> current;
    static std::once_flag initializeCurrentIdentifier;
    std::call_once(initializeCurrentIdentifier, [] {
        current.construct(0);
    });
    return ++current.get();
}

} // namespace WebCore

namespace WTF {

template<> struct HashTraits<WebCore::ElementIdentifier> : SimpleClassHashTraits<WebCore::ElementIdentifier> { };

template<> struct DefaultHash<WebCore::ElementIdentifier> {
    typedef WebCore::ElementIdentifierHash Hash;
};

} // namespace WTF
