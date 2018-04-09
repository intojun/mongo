/**
 *    Copyright (C) 2017 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/db/pipeline/resume_token.h"

#include <algorithm>
#include <boost/optional/optional_io.hpp>

#include "mongo/db/pipeline/document.h"
#include "mongo/db/pipeline/document_source_change_stream.h"
#include "mongo/unittest/unittest.h"

namespace mongo {

namespace {

using Format = ResumeToken::SerializationFormat;

TEST(ResumeToken, EncodesFullTokenFromData) {
    Timestamp ts(1000, 2);
    UUID testUuid = UUID::gen();
    Document documentKey{{"_id"_sd, "stuff"_sd}, {"otherkey"_sd, Document{{"otherstuff"_sd, 2}}}};

    ResumeTokenData resumeTokenDataIn(ts, Value(documentKey), testUuid);
    ResumeToken token(resumeTokenDataIn);
    ResumeTokenData tokenData = token.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, EncodesTimestampOnlyTokenFromData) {
    Timestamp ts(1001, 3);

    ResumeTokenData resumeTokenDataIn;
    resumeTokenDataIn.clusterTime = ts;
    ResumeToken token(resumeTokenDataIn);
    ResumeTokenData tokenData = token.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, ShouldRoundTripThroughHexStringEncoding) {
    Timestamp ts(1000, 2);
    UUID testUuid = UUID::gen();
    Document documentKey{{"_id"_sd, "stuff"_sd}, {"otherkey"_sd, Document{{"otherstuff"_sd, 2}}}};

    ResumeTokenData resumeTokenDataIn(ts, Value(documentKey), testUuid);

    // Test serialization/parsing through Document.
    auto rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kHexString));
    ResumeTokenData tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);

    // Test serialization/parsing through BSON.
    rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kHexString).toBson());
    tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, ShouldRoundTripThroughBinDataEncoding) {
    Timestamp ts(1000, 2);
    UUID testUuid = UUID::gen();
    Document documentKey{{"_id"_sd, "stuff"_sd}, {"otherkey"_sd, Document{{"otherstuff"_sd, 2}}}};

    ResumeTokenData resumeTokenDataIn(ts, Value(documentKey), testUuid);

    // Test serialization/parsing through Document.
    auto rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kBinData).toBson());
    ResumeTokenData tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);

    // Test serialization/parsing through BSON.
    rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kBinData).toBson());
    tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, TimestampOnlyTokenShouldRoundTripThroughHexStringEncoding) {
    Timestamp ts(1001, 3);

    ResumeTokenData resumeTokenDataIn;
    resumeTokenDataIn.clusterTime = ts;

    // Test serialization/parsing through Document.
    auto rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kHexString).toBson());
    ResumeTokenData tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);

    // Test serialization/parsing through BSON.
    rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kHexString).toBson());
    tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, TimestampOnlyTokenShouldRoundTripThroughBinDataEncoding) {
    Timestamp ts(1001, 3);

    ResumeTokenData resumeTokenDataIn;
    resumeTokenDataIn.clusterTime = ts;

    // Test serialization/parsing through Document.
    auto rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kBinData).toBson());
    ResumeTokenData tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);

    // Test serialization/parsing through BSON.
    rtToken =
        ResumeToken::parse(ResumeToken(resumeTokenDataIn).toDocument(Format::kBinData).toBson());
    tokenData = rtToken.getData();
    ASSERT_EQ(resumeTokenDataIn, tokenData);
}

TEST(ResumeToken, TestMissingTypebitsOptimization) {
    Timestamp ts(1000, 1);
    UUID testUuid = UUID::gen();

    ResumeTokenData hasTypeBitsData(ts, Value(Document{{"_id", 1.0}}), testUuid);
    ResumeTokenData noTypeBitsData(ResumeTokenData(ts, Value(Document{{"_id", 1}}), testUuid));
    ResumeToken hasTypeBitsToken(hasTypeBitsData);
    ResumeToken noTypeBitsToken(noTypeBitsData);
    ASSERT_EQ(noTypeBitsToken, hasTypeBitsToken);
    auto hasTypeBitsDoc = hasTypeBitsToken.toDocument(Format::kHexString);
    auto noTypeBitsDoc = noTypeBitsToken.toDocument(Format::kHexString);
    ASSERT_FALSE(hasTypeBitsDoc["_typeBits"].missing());
    ASSERT_TRUE(noTypeBitsDoc["_typeBits"].missing()) << noTypeBitsDoc["_typeBits"];
    auto rtHasTypeBitsData = ResumeToken::parse(hasTypeBitsDoc).getData();
    auto rtNoTypeBitsData = ResumeToken::parse(noTypeBitsDoc).getData();
    ASSERT_EQ(hasTypeBitsData, rtHasTypeBitsData);
    ASSERT_EQ(noTypeBitsData, rtNoTypeBitsData);
    ASSERT_EQ(BSONType::NumberDouble, rtHasTypeBitsData.documentKey["_id"].getType());
    ASSERT_EQ(BSONType::NumberInt, rtNoTypeBitsData.documentKey["_id"].getType());
}


TEST(ResumeToken, CorruptTokens) {
    // Missing document.
    ASSERT_THROWS(ResumeToken::parse(Document()), AssertionException);
    // Missing data field.
    ASSERT_THROWS(ResumeToken::parse(Document{{"somefield"_sd, "stuff"_sd}}), AssertionException);
    // Wrong type data field
    ASSERT_THROWS(ResumeToken::parse(Document{{"_data"_sd, BSONNULL}}), AssertionException);
    ASSERT_THROWS(ResumeToken::parse(Document{{"_data"_sd, 0}}), AssertionException);

    // Valid data field, but wrong type typeBits.
    Timestamp ts(1010, 4);
    ResumeTokenData tokenData;
    tokenData.clusterTime = ts;
    auto goodTokenDocBinData = ResumeToken(tokenData).toDocument(Format::kBinData);
    auto goodData = goodTokenDocBinData["_data"].getBinData();
    ASSERT_THROWS(ResumeToken::parse(Document{{"_data"_sd, goodData}, {"_typeBits", "string"_sd}}),
                  AssertionException);
    auto goodTokenDocString = ResumeToken(tokenData).toDocument(Format::kHexString);
    auto goodString = goodTokenDocString["_data"].getString();
    ASSERT_THROWS(
        ResumeToken::parse(Document{{"_data"_sd, goodString}, {"_typeBits", "string"_sd}}),
        AssertionException);

    // Valid data except wrong bindata type.
    ASSERT_THROWS(ResumeToken::parse(
                      Document{{"_data"_sd, BSONBinData(goodData.data, goodData.length, newUUID)}}),
                  AssertionException);
    // Valid data, wrong typeBits bindata type.
    ASSERT_THROWS(
        ResumeToken::parse(Document{{"_data"_sd, goodData},
                                    {"_typeBits", BSONBinData(goodData.data, 0, newUUID)}}),
        AssertionException);

    const unsigned char zeroes[] = {0, 0, 0, 0, 0};
    const unsigned char nonsense[] = {165, 85, 77, 86, 255};
    // Data of correct type, but empty.  This won't fail until we try to decode the data.
    auto emptyToken =
        ResumeToken::parse(Document{{"_data"_sd, BSONBinData(zeroes, 0, BinDataGeneral)}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);
    emptyToken = ResumeToken::parse(Document{{"_data"_sd, "string"_sd}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);

    // Data of correct type with a bunch of zeros.
    auto zeroesToken =
        ResumeToken::parse(Document{{"_data"_sd, BSONBinData(zeroes, 5, BinDataGeneral)}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);
    zeroesToken = ResumeToken::parse(Document{{"_data"_sd, "00000"_sd}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);

    // Data of correct type with a bunch of nonsense.
    auto nonsenseToken =
        ResumeToken::parse(Document{{"_data"_sd, BSONBinData(nonsense, 5, BinDataGeneral)}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);
    nonsenseToken = ResumeToken::parse(Document{{"_data"_sd, "nonsense"_sd}});
    ASSERT_THROWS(emptyToken.getData(), AssertionException);

    // Valid data, bad typeBits; note that an all-zeros typebits is valid so it is not tested here.
    auto badTypeBitsToken = ResumeToken::parse(
        Document{{"_data"_sd, goodData}, {"_typeBits", BSONBinData(nonsense, 5, BinDataGeneral)}});
    ASSERT_THROWS(badTypeBitsToken.getData(), AssertionException);
}

TEST(ResumeToken, StringEncodingSortsCorrectly) {
    // Make sure that the string encoding of the resume tokens will compare in the correct order,
    // namely timestamp, uuid, then documentKey.
    Timestamp ts2_2(2, 2);
    Timestamp ts10_4(10, 4);
    Timestamp ts10_5(10, 5);
    Timestamp ts11_3(11, 3);

    // Generate two different UUIDs, and figure out which one is smaller. Store the smaller one in
    // 'lower_uuid'.
    UUID lower_uuid = UUID::gen();
    UUID higher_uuid = UUID::gen();
    if (lower_uuid > higher_uuid) {
        std::swap(lower_uuid, higher_uuid);
    }

    auto assertLt = [](const ResumeTokenData& lower, const ResumeTokenData& higher) {
        auto lowerString = ResumeToken(lower).toDocument(Format::kHexString)["_data"].getString();
        auto higherString = ResumeToken(higher).toDocument(Format::kHexString)["_data"].getString();
        ASSERT_LT(lowerString, higherString);
    };

    // Test using only Timestamps.
    assertLt({ts2_2, Value(), boost::none}, {ts10_4, Value(), boost::none});
    assertLt({ts2_2, Value(), boost::none}, {ts10_5, Value(), boost::none});
    assertLt({ts2_2, Value(), boost::none}, {ts11_3, Value(), boost::none});
    assertLt({ts10_4, Value(), boost::none}, {ts10_5, Value(), boost::none});
    assertLt({ts10_4, Value(), boost::none}, {ts11_3, Value(), boost::none});
    assertLt({ts10_5, Value(), boost::none}, {ts11_3, Value(), boost::none});

    // Test that the Timestamp is more important than the UUID and documentKey.
    assertLt({ts10_4, Value(Document{{"_id", 0}}), lower_uuid},
             {ts10_5, Value(Document{{"_id", 0}}), lower_uuid});
    assertLt({ts2_2, Value(Document{{"_id", 0}}), lower_uuid},
             {ts10_5, Value(Document{{"_id", 0}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 1}}), lower_uuid},
             {ts10_5, Value(Document{{"_id", 0}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 0}}), higher_uuid},
             {ts10_5, Value(Document{{"_id", 0}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 0}}), lower_uuid},
             {ts10_5, Value(Document{{"_id", 0}}), higher_uuid});

    // Test that when the Timestamp is the same, the UUID breaks the tie.
    assertLt({ts2_2, Value(Document{{"_id", 0}}), lower_uuid},
             {ts2_2, Value(Document{{"_id", 0}}), higher_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 0}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", 0}}), higher_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 1}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", 0}}), higher_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 1}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", 2}}), higher_uuid});

    // Test that when the Timestamp and the UUID are the same, the documentKey breaks the tie.
    assertLt({ts2_2, Value(Document{{"_id", 0}}), lower_uuid},
             {ts2_2, Value(Document{{"_id", 1}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 0}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", 1}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", 1}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", "string"_sd}}), lower_uuid});
    assertLt({ts10_4, Value(Document{{"_id", BSONNULL}}), lower_uuid},
             {ts10_4, Value(Document{{"_id", 0}}), lower_uuid});
}

}  // namspace
}  // namspace mongo