//
// Created by andrey on 23.01.17.
//

#include "io_support.hpp"
#include "modules/path_extend/pe_utils.hpp"

namespace path_extend {

void path_extend::TranscriptToGeneJoiner::MakeSet(size_t x) {
    parents_[x] = x;
    ranks_[x] = 0;
}

void path_extend::TranscriptToGeneJoiner::JoinTrees(size_t x, size_t y) {
    x = FindTree(x);
    y = FindTree(y);
    if (x != y) {
        if (ranks_[x] < ranks_[y])
            parents_[x] = y;
        else
            parents_[y] = x;
        if (ranks_[x] == ranks_[y])
            ++ranks_[x];
    }
}

void path_extend::TranscriptToGeneJoiner::Init(const PathContainer &paths) {
    DEBUG("Initializing parents and ranks");
    parents_.resize(paths.size());
    ranks_.resize(paths.size());

    size_t path_num = 0;
    for (auto iter = paths.begin(); iter != paths.end(); ++iter, ++path_num) {
        path_id_[iter.get()] = path_num;
        path_id_[iter.getConjugate()] = path_num;
        MakeSet(path_num);
    }

    DEBUG("Initialized parents and ranks");

    VERIFY_MSG(path_num == paths.size(), "Path Num " << path_num << " Size " << paths.size())
}

size_t path_extend::TranscriptToGeneJoiner::FindTree(size_t x) {
    size_t parent;
    if (x == parents_[x]) {
        parent = x;
    }
    else {
        parents_[x] = FindTree(parents_[x]);
        parent = parents_[x];
    }
    return parent;
}

size_t path_extend::TranscriptToGeneJoiner::GetPathId(BidirectionalPath *path) {
    return path_id_[path];
}

void path_extend::TranscriptToGeneJoiner::Construct(const PathContainer &paths) {
    Init(paths);

    GraphCoverageMap edges_coverage(g_, paths);

    DEBUG("Union trees");
    //For all edges in coverage map
    for (auto iterator = edges_coverage.begin(); iterator != edges_coverage.end(); ++iterator) {
        //Select a path covering an edge
        EdgeId edge = iterator->first;
        GraphCoverageMap::MapDataT *edge_paths = iterator->second;

        if (g_.length(edge) > min_edge_len_ && edge_paths->size() > 1) {
            DEBUG("Long edge " << edge.int_id() << " Paths " << edge_paths->size());
            //For all other paths covering this edge join then into single gene with the first path
            for (auto it_edge = ++edge_paths->begin(); it_edge != edge_paths->end(); ++it_edge) {
                size_t first = path_id_[*edge_paths->begin()];
                size_t next = path_id_[*it_edge];
                DEBUG("Edge " << edge.int_id() << " First " << first << " Next " << next);

                JoinTrees(first, next);
            }
        }
    }
}

string path_extend::IOContigStorage::ToString(const BidirectionalPath &path) const {
    stringstream ss;
    if (path.IsInterstrandBulge() && path.Size() == 1) {
        ss << g_.EdgeNucls(path.Back()).Subseq(k_);
        return ss.str();
    }

    if (!path.Empty()) {
        ss << g_.EdgeNucls(path[0]).Subseq(0, k_);
    }

    size_t i = 0;
    while (i < path.Size()) {
        //FIXME shouldn't we consider future right end trimming here
        int offset = 0;
        while (i < path.Size() && offset >= (int) g_.length(path[i]) + path.GapAt(i)) {
            offset -= (int) g_.length(path[i]) + path.GapAt(i);
            ++i;
        }
        if (i == path.Size()) {
            break;
        }

        int overlap_size = offset + (int) k_ - path.GapAt(i);

        if (overlap_size < 0) {
            for (size_t j = 0; j < abs(overlap_size); ++j) {
                ss << "N";
            }
            overlap_size = 0;
        }

        size_t right_end = g_.length(path[i]) + g_.k();
        if (i != path.Size() - 1) {
            VERIFY(right_end > path.TrashPreviousAt(i + 1));
            right_end -= path.TrashPreviousAt(i + 1);
        }

        if (int(right_end) < overlap_size) {
            //FIXME this might be a weird case resulting in wrong offsets
            break;
        }
        ss << g_.EdgeNucls(path[i]).Subseq(overlap_size, right_end);
        ++i;
    }
    return ss.str();
}

void path_extend::ScaffoldBreaker::SplitPath(const BidirectionalPath &path, PathContainer &result) const {
    size_t i = 0;

    while (i < path.Size()) {
        BidirectionalPath *p = new BidirectionalPath(path.graph(), path[i]);
        ++i;

        while (i < path.Size() and path.GapAt(i) <= min_gap_) {
            p->PushBack(path[i], path.GapAt(i), path.TrashPreviousAt(i), path.TrashCurrentAt(i));
            ++i;
        }

        if (i < path.Size()) {
            DEBUG("split path " << i << " gap " << path.GapAt(i));
            p->Print();
        }

        BidirectionalPath *cp = new BidirectionalPath(p->Conjugate());
        result.AddPair(p, cp);
    }
}

void path_extend::ScaffoldBreaker::Break(const PathContainer &paths, PathContainer &result) const {
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        SplitPath(*it.get(), result);
    }
    result.SortByLength();
}

}

