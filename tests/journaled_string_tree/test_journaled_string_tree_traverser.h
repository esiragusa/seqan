// ==========================================================================
//                 SeqAn - The Library for Sequence Analysis
// ==========================================================================
// Copyright (c) 2006-2015, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Rene Rahn <rene.rahn@fu-berlin.de>
// ==========================================================================
// Tests for the journaled string tree.
// ==========================================================================

#ifndef TESTS_JOURNALED_STRING_TREE_TEST_JOURNALED_STRING_TREE_TRAVERSER_H_
#define TESTS_JOURNALED_STRING_TREE_TEST_JOURNALED_STRING_TREE_TRAVERSER_H_

#include <sstream>

#include <seqan/basic.h>
#include <seqan/journaled_string_tree.h>
#include <seqan/stream.h>
#include <seqan/vcf_io.h>

#include "test_config_reader.h"
//#include "test_journaled_string_tree.h"

using namespace seqan;

struct TestJstPosConfig_
{
    typedef __uint16 TDeltaPos;
    typedef Dna      TSnpValue;
    typedef __uint16 TDelValue;
    typedef String<Dna> TInsValue;
    typedef Pair<TDelValue, TInsValue> TSVValue;
};

template <typename TString, typename TRef, typename TDeltaMap>
inline void
_createJournaledStrings(StringSet<TString> & set,
                        TRef const & source,
                        TDeltaMap const & map,
                        unsigned const dim)
{

    for (unsigned seqId = 0; seqId < dim; ++seqId)
    {
        TString seq = source;
        auto it = end(map, Standard());
        auto itBegin = begin(map, Standard());

        while (it != itBegin)
        {
            --it;
            if (!getDeltaCoverage(*it)[seqId])
                continue;
            switch (getDeltaType(*it))
            {
                case DELTA_TYPE_SNP:
                {
                    erase(seq, getDeltaPosition(*it));
                    insertValue(seq, getDeltaPosition(*it), deltaValue(it, DeltaTypeSnp()));
                    break;
                }
                case DELTA_TYPE_DEL:
                {
                    if((*it).deltaTypeEnd != DeltaEndType::IS_RIGHT)
                        erase(seq, getDeltaPosition(*it), getDeltaPosition(*it)+ deltaValue(it, DeltaTypeDel()));
                    break;
                }
                case DELTA_TYPE_INS:
                {
                    insert(seq, getDeltaPosition(*it), deltaValue(it, DeltaTypeIns()));
                    break;
                }
                case DELTA_TYPE_SV:
                {
                    if((*it).deltaTypeEnd != DeltaEndType::IS_RIGHT)
                    {
                        erase(seq, getDeltaPosition(*it), getDeltaPosition(*it) + deltaValue(it, DeltaTypeSV()).i1);
                        insert(seq, getDeltaPosition(*it), deltaValue(it, DeltaTypeSV()).i2);
                    }
                    break;
                }
            }
        }
        appendValue(set, seq);
    }
}

template <typename TFile>
inline auto
_createJstFromFile(TJst & jst, TInFile & configIn) -> decltype(_createJstFromFile(path))
{
    while (!atEnd(configIn))
    {
        TestConfigRecord_<Dna, unsigned> record;
        readRecord(record, configIn);

        switch (record.deltaType)
        {
            case DELTA_TYPE_SNP:
            {
                insert(jst, record.pos, record.snp, record.coverage, DeltaTypeSnp());
                break;
            }
            case DELTA_TYPE_DEL:
            {
                insert(jst, record.pos, record.del, record.coverage, DeltaTypeDel());
                break;
            }
            case DELTA_TYPE_INS:
            {
                insert(jst, record.pos, record.ins, record.coverage, DeltaTypeIns());
                break;
            }
            case DELTA_TYPE_SV:
            {
                insert(jst, record.pos, record.stv, record.coverage, DeltaTypeSV());
                break;
            }
        }
    }
}

template <typename TJst>
inline TJst _createSimpleJst()
{
    typename Host<typename Member<TJst, JstSourceMember>::Type>::Type const seq = "AGATCGAGCGAGCTAGCGACTCAG";
    TJst jst(seq, 100);
    String<unsigned> ids;
    appendValue(ids, 0);
    appendValue(ids, 3);
    appendValue(ids, 9);
    appendValue(ids, 99);

    insert(jst, 1, 3, ids, DeltaTypeDel());
    insert(jst, 8, "CGTA", ids, DeltaTypeIns());
    insert(jst, 10, 'C', ids, DeltaTypeSnp());
    insert(jst, 15, 2, ids, DeltaTypeDel());
    insert(jst, 20, 'A', ids, DeltaTypeSnp());

//    SEQAN_ASSERT(create(jst));
    return jst;
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_constructor)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    {  // Default ctor.
        TTraverser traverser;
        SEQAN_ASSERT(traverser._contPtr == nullptr);
        SEQAN_ASSERT(traverser._branchLength == 1);
        SEQAN_ASSERT(traverser._contextSize == 1);
        SEQAN_ASSERT(traverser._stackPtr.get() != nullptr);
        SEQAN_ASSERT(empty(*traverser._stackPtr) == true);
    }

    {  // Constructor with jst
        TJst jst = _createSimpleJst<TJst>();

        TTraverser traverser(jst);
        SEQAN_ASSERT(traverser._contPtr == &jst);
        SEQAN_ASSERT(traverser._branchLength == 1u);
        SEQAN_ASSERT(traverser._contextSize == 1u);
        SEQAN_ASSERT(traverser._stackPtr.get() != nullptr);
        SEQAN_ASSERT(length(*traverser._stackPtr) == 1u);
    }

    {  // Copy C'tor

    }
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_traverser)
{
    typedef JournaledStringTree<DnaString> TJst;

    // Test Traverser Metafunction!
    TJst jst = _createSimpleJst<TJst>();

    { // Without observable
        typename Traverser<TJst>::Type subject(jst);
        SEQAN_ASSERT(subject._contPtr == &jst);
        SEQAN_ASSERT(subject._contextSize == 1);
        SEQAN_ASSERT(length(*subject._stackPtr) == 1);
    }
    { // With observable
        // TODO(rrahn): Write me!
    }
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_init)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();

    {
        TTraverser test;
        init(test, jst, 1);

        SEQAN_ASSERT(test._contPtr == &jst);
        SEQAN_ASSERT(test._branchLength == 1u);
        SEQAN_ASSERT(test._contextSize == 1u);
        SEQAN_ASSERT(length(*test._stackPtr) == 1u);
        SEQAN_ASSERT_EQ(*(back(*test._stackPtr).curEdgeIt), 'A');
    }

    {
        TTraverser test;
        init(test, jst, 5);

        SEQAN_ASSERT(test._contPtr == &jst);
        SEQAN_ASSERT(test._branchLength == 5u);
        SEQAN_ASSERT(test._contextSize == 5u);
        SEQAN_ASSERT(length(*test._stackPtr) == 3u);
        SEQAN_ASSERT(*(back(*test._stackPtr).curEdgeIt) == 'G');
    }

    {
        TTraverser test;
        init(test, jst, 10);

        SEQAN_ASSERT(test._contPtr == &jst);
        SEQAN_ASSERT(test._branchLength == 10u);
        SEQAN_ASSERT(test._contextSize == 10u);
        SEQAN_ASSERT_EQ(length(*test._stackPtr), 4u);
        SEQAN_ASSERT(*(back(*test._stackPtr).curEdgeIt) == 'C');
    }

}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_container)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();

    TTraverser test(jst);
    SEQAN_ASSERT_EQ(&container(test), &jst);

    TTraverser const testC = test;
    SEQAN_ASSERT_EQ(&container(test), &jst);
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_context_iterator)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();
    TTraverser test(jst);

    SEQAN_ASSERT(contextIterator(test) == begin(impl::member(jst, JstSourceMember()), Standard()));
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_go_next)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();
    TTraverser test(jst, 1, 10);

    SEQAN_ASSERT(*contextIterator(test) == 'A');
    goNext(test);
    SEQAN_ASSERT(*contextIterator(test) == 'C');
    goNext(test, 6);
    SEQAN_ASSERT(*contextIterator(test) == 'T');
    goNext(test, 2);
    SEQAN_ASSERT(*contextIterator(test) == 'C');
    goNext(test, 7);
    SEQAN_ASSERT(*contextIterator(test) == 'A');
    goNext(test, 10);
    SEQAN_ASSERT(*contextIterator(test) == 'G');
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_at_end)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();
    TTraverser test(jst);

    SEQAN_ASSERT_EQ(atEnd(test), false);
    for (unsigned i = 0; i < 28; ++i)
    {
        goNext(test);
    }
    SEQAN_ASSERT_EQ(atEnd(test), true);
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_context_size)
{
    typedef JournaledStringTree<DnaString> TJst;
    typedef TraverserImpl<TJst, JstTraversalSpec<> > TTraverser;

    TJst jst = _createSimpleJst<TJst>();

    TTraverser test(jst);

    SEQAN_ASSERT_EQ(contextSize(test), 1u);

    init(test, jst, 5);
    SEQAN_ASSERT_EQ(contextSize(test), 5u);
}

SEQAN_DEFINE_TEST(test_journaled_string_tree_traverser_basic_traversal)
{
    CharString path = SEQAN_PATH_TO_ROOT();
    append(path, "/tests/journaled_string_tree/testConfig.vcf");

    auto jst = _createJstFromFile(path);

    TestConfigFileIn_ configIn(toCString(path));

    TestConfigHeader_<DnaString> header;
    readHeader(header, configIn);
    DnaString seq = header.ref;

    JournaledStringTree<DnaString, TestJstPosConfig_> jst(seq, length(context(configIn)));
    _createJst(jst, configIn);

//    SEQAN_ASSERT(create(jst));

    StringSet<DnaString> testSeqs;
    resize(testSeqs, length(jst));

    std::stringstream strStream;


    typename Traverser<JournaledStringTree<DnaString, TestJstPosConfig_> >::Type sub(jst, 1, 10);

    while (!atEnd(sub))
    {
        unsigned count = 0;
        for (auto it = begin(back(*sub._stackPtr).coverage); it != end(back(*sub._stackPtr).coverage); ++it, ++count)
            if (*it)
                appendValue(testSeqs[count], *(back(*sub._stackPtr).curEdgeIt));

//        std::cout << "Current Sequences: " << std::endl;
//        count = 0;
//        for (auto seq : testSeqs)
//        {
//            std::cout << "seq ";
//            std::cout.fill('0');
//            std::cout.width(2);
//            std::cout << count << ": ";
//            std::cout << seq << '\n';
//            ++count;
//        }
        goNext(sub);
        
    }

    for (unsigned i = 0; i < length(impl::buffer(sub)._journaledSet); ++i)
    {
//        std::cout << testSeqs[i] << std::endl;
        SEQAN_ASSERT_MSG(testSeqs[i] == impl::buffer(sub)._journaledSet[i], "Sequence %d did not match", i);
    }
}

#endif // EXTRAS_TESTS_JOURNALED_STRING_TREE_TEST_JOURNALED_STRING_TREE_TRAVERSER_H_
