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

enum PageIdentifierType { };
//using PageIdentifier = ObjectIdentifier<PageIdentifierType>;

class PageIdentifierBase {
protected:
    WTF_EXPORT_PRIVATE static uint64_t generateIdentifierInternal();
    WTF_EXPORT_PRIVATE static uint64_t generateThreadSafeIdentifierInternal();
};

class PageIdentifier : private PageIdentifierBase {
public:
    static PageIdentifier generate()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return PageIdentifier { generateIdentifierInternal() };
    }

    static PageIdentifier generateThreadSafe()
    {
        RELEASE_ASSERT(!m_generationProtected);
        return PageIdentifier { generateThreadSafeIdentifierInternal() };
    }

    static void enableGenerationProtection()
    {
        m_generationProtected = true;
    }

    PageIdentifier() = default;

    PageIdentifier(WTF::HashTableDeletedValueType) : m_identifier(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_identifier == hashTableDeletedValue(); }

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        ASSERT(isValidIdentifier(m_identifier));
        encoder << m_identifier;
    }
    template<typename Decoder> static Optional<PageIdentifier> decode(Decoder& decoder)
    {
        Optional<uint64_t> identifier;
        decoder >> identifier;
        if (!identifier || !isValidIdentifier(*identifier))
            return WTF::nullopt;
        return PageIdentifier { *identifier };
    }

    bool operator==(const PageIdentifier& other) const
    {
        return m_identifier == other.m_identifier;
    }

    bool operator!=(const PageIdentifier& other) const
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
        static bool isEmptyValue(PageIdentifier identifier)
        {
            return !identifier.m_identifier;
        }

        static constexpr PageIdentifier emptyValue()
        {
            return PageIdentifier();
        }
    };

private:
    friend PageIdentifier makePageIdentifier(uint64_t);
    friend struct HashTraits<PageIdentifier>;
    friend struct PageIdentifierHash;

    static uint64_t hashTableDeletedValue() { return std::numeric_limits<uint64_t>::max(); }
    static bool isValidIdentifier(uint64_t identifier) { return identifier && identifier != hashTableDeletedValue(); }

    explicit constexpr PageIdentifier(uint64_t identifier)
        : m_identifier(identifier)
    {
    }

    uint64_t m_identifier { 0 };
    inline static bool m_generationProtected { false };
};

inline PageIdentifier makePageIdentifier(uint64_t identifier)
{
    return PageIdentifier { identifier };
}

struct PageIdentifierHash {
    static unsigned hash(const PageIdentifier& identifier) { return WTF::intHash(identifier.m_identifier); }
    static bool equal(const PageIdentifier& a, const PageIdentifier& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

inline TextStream& operator<<(TextStream& ts, const PageIdentifier& identifier)
{
    ts << identifier.toUInt64();
    return ts;
}

inline uint64_t PageIdentifierBase::generateIdentifierInternal()
{
    static uint64_t current;
    return ++current;
}

inline uint64_t PageIdentifierBase::generateThreadSafeIdentifierInternal()
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

template<> struct HashTraits<WebCore::PageIdentifier> : SimpleClassHashTraits<WebCore::PageIdentifier> { };

template<> struct DefaultHash<WebCore::PageIdentifier> {
    typedef WebCore::PageIdentifierHash Hash;
};

} // namespace WTF
