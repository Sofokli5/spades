//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once

#include "graph_core.hpp"
#include "utils/verify.hpp"
#include "utils/logger/logger.hpp"
#include "sequence/sequence_tools.hpp"

#include <llvm/ADT/PointerSumType.h>
#include <llvm/ADT/PointerEmbeddedInt.h>

#include <utility>
#include <vector>
#include <set>
#include <cstring>
#include <cstdint>

namespace debruijn_graph {
class DeBruijnDataMaster;

struct Link {
  typedef omnigraph::impl::EdgeId EdgeId;
  Link(const std::pair<EdgeId, EdgeId> &link, unsigned overlap) : link(link), overlap(overlap) {}

  std::pair<EdgeId, EdgeId> link;
  unsigned overlap;
};

class DeBruijnVertexData {
    friend class DeBruijnDataMaster;
    typedef std::shared_ptr<Link> LinkPtr;


    enum OverlapKind {
        ComplexOverlap,
        ExplicitOverlap
    };

    struct OverlapStorage {
      OverlapStorage(): links() {}
      OverlapStorage(const std::vector<LinkPtr> &other_links): links(other_links) {}

      ~OverlapStorage() {
          links.clear();
      }

      void AddLinks(const std::vector<LinkPtr> &other_links) {
          for (const auto &link: other_links) {
              links.push_back(link);
          }
      }

      void AddLink(LinkPtr added_link) {
          links.push_back(added_link);
      }

      std::vector<LinkPtr> GetLinks() const {
          return links;
      }

      std::vector<LinkPtr> MoveLinks() {
          auto links_copy = links;
          links.clear();
          return links_copy;
      }
      std::vector<LinkPtr> links;
    };

    typedef llvm::PointerEmbeddedInt<uint32_t, 32> SimpleOverlap;
    typedef llvm::PointerSumType<OverlapKind,
                                 llvm::PointerSumTypeMember<ComplexOverlap, OverlapStorage*>,
                                 llvm::PointerSumTypeMember<ExplicitOverlap, SimpleOverlap>> Overlap;

    Overlap overlap_;

public:
    explicit DeBruijnVertexData(const std::vector<LinkPtr> &links)
            : overlap_(Overlap::create<ComplexOverlap>(new OverlapStorage(links))) {}
//    {
//        INFO("New storage")
//        auto *storage = new OverlapStorage(links);
//        INFO("Creating");
//        overlap_ = Overlap::create<ComplexOverlap>(storage);
//        INFO("kek");
//    }
    explicit DeBruijnVertexData(unsigned overlap = 0)
            : overlap_(Overlap::create<ExplicitOverlap>(overlap)) {}
    explicit DeBruijnVertexData(OverlapStorage *storage)
            : overlap_(Overlap::create<ComplexOverlap>(storage)) {}

    ~DeBruijnVertexData() {
//        if (has_complex_overlap())
//            delete complex_overlap();
    }

    void set_overlap(unsigned overlap) {
        overlap_.set<ExplicitOverlap>(overlap);
    }

    unsigned overlap() const {
        return overlap_.get<ExplicitOverlap>();
    }

    std::vector<LinkPtr> get_links() const {
        return overlap_.get<ComplexOverlap>()->GetLinks();
    }

    std::vector<LinkPtr> move_links() {
        return overlap_.get<ComplexOverlap>()->MoveLinks();
    }

    void add_link(LinkPtr link) {
        overlap_.get<ComplexOverlap>()->AddLink(link);
    }

    void add_links(const std::vector<LinkPtr> &links) {
        overlap_.get<ComplexOverlap>()->AddLinks(links);
    }

    bool has_complex_overlap() const {
        return overlap_.is<ComplexOverlap>();
    }

    OverlapStorage *complex_overlap() {
        return overlap_.get<ComplexOverlap>();
    }
};

class CoverageData {
 private:
    uint32_t coverage_;

 public:
    CoverageData()
            : coverage_(0) {
    }

    void inc_coverage(int value) {
        VERIFY(value >= 0 || coverage_ > unsigned(-value));
        coverage_ += value;
    }

    void set_coverage(unsigned coverage) {
        coverage_ = coverage;
    }

    //not length normalized
    unsigned coverage() const {
        return coverage_;
    }
};

class DeBruijnEdgeData {
    friend class DeBruijnDataMaster;
    CoverageData coverage_;
    CoverageData flanking_cov_;
    Sequence nucls_;
public:

    explicit DeBruijnEdgeData(const Sequence &nucls) :
            nucls_(nucls) {}

    const Sequence& nucls() const {
        return nucls_;
    }

    void inc_raw_coverage(int value) {
        coverage_.inc_coverage(value);
    }

    void set_raw_coverage(unsigned coverage) {
        coverage_.set_coverage(coverage);
    }

    unsigned raw_coverage() const {
        return coverage_.coverage();
    }

    void inc_flanking_coverage(int value) {
        flanking_cov_.inc_coverage(value);
    }

    void set_flanking_coverage(unsigned flanking_coverage) {
        flanking_cov_.set_coverage(flanking_coverage);
    }

    //not length normalized
    unsigned flanking_coverage() const {
        return flanking_cov_.coverage();
    }

    size_t size() const {
        return nucls_.size();
    }
};

class DeBruijnDataMaster {
private:
    unsigned k_;

public:
    typedef DeBruijnVertexData VertexData;
    typedef DeBruijnEdgeData EdgeData;
    typedef DeBruijnVertexData::LinkPtr LinkPtr;
    typedef DeBruijnVertexData::OverlapStorage OverlapStorage;

    DeBruijnDataMaster(unsigned k)
            : k_(k) {}

    const EdgeData MergeData(const std::vector<const EdgeData *> &to_merge, const std::vector<uint32_t> &overlaps,
                             bool safe_merging = true) const;

    std::pair<VertexData, std::pair<EdgeData, EdgeData>> SplitData(const EdgeData& edge, size_t position, bool is_self_conj = false) const;

    EdgeData GlueData(const EdgeData&, const EdgeData& data2) const;

    bool isSelfConjugate(const EdgeData &data) const {
        return data.nucls() == !(data.nucls());
    }

    EdgeData conjugate(const EdgeData &data) const {
        return EdgeData(!(data.nucls()));
    }

    VertexData conjugate(const VertexData &data) const {
        return data;
    }

    size_t length(const EdgeData& data) const {
        return data.nucls().size() - k_;
    }

    // FIXME: make use of it!
    size_t length(const VertexData &data) const {
        return data.overlap();
    }

    unsigned k() const {
        return k_;
    }

    void set_k(unsigned k) {
        k_ = k;
    }
};

//typedef DeBruijnVertexData VertexData;
//typedef DeBruijnEdgeData EdgeData;
//typedef DeBruijnDataMaster DataMaster;

inline const DeBruijnEdgeData DeBruijnDataMaster::MergeData(const std::vector<const EdgeData *> &to_merge,
                                                            const std::vector<uint32_t> &overlaps,
                                                            bool safe_merging) const {
    std::vector<Sequence> ss;
    ss.reserve(to_merge.size());
    for (auto it = to_merge.begin(); it != to_merge.end(); ++it) {
        ss.push_back((*it)->nucls());
    }
    return EdgeData(MergeOverlappingSequences(ss, overlaps, safe_merging));
}

inline std::pair<DeBruijnVertexData, std::pair<DeBruijnEdgeData, DeBruijnEdgeData>> DeBruijnDataMaster::SplitData(const EdgeData& edge,
                                                                                                                  size_t position,
                                                                                                                  bool is_self_conj) const {
    const Sequence& nucls = edge.nucls();
    size_t end = nucls.size();
    if (is_self_conj) {
        VERIFY(position < end);
        end -= position;
    }
    return std::make_pair(VertexData(), std::make_pair(EdgeData(edge.nucls().Subseq(0, position + k_)), EdgeData(nucls.Subseq(position, end))));
}

inline DeBruijnEdgeData DeBruijnDataMaster::GlueData(const DeBruijnEdgeData&, const DeBruijnEdgeData& data2) const {
    return data2;
}

}
