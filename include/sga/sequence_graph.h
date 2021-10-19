#ifndef SGA_SEQUENCEGRAPH_H
#define SGA_SEQUENCEGRAPH_H

#include <algorithm>
#include <iterator>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "sequence.h"
#include "utils.h"

namespace sga {
template <class GraphSizeType = int32_t, class QueryLengthType = int16_t, class ScoreType = int16_t>
class SequenceGraph {
 public:
  SequenceGraph() {}
  ~SequenceGraph() {}
  GraphSizeType GetNumVerticesInCompactedGraph() {
    return compacted_graph_labels_.size();
  }
  GraphSizeType GetNumEdgesInCompactedGraph() {
    GraphSizeType num_edges = 0;
    for (std::vector<GraphSizeType> &neighbors : compacted_graph_adjacency_list_) {
      num_edges += neighbors.size();
    }
    return num_edges;
  }
  GraphSizeType GetNumVertices() {
    return labels_.size();
  }
  GraphSizeType GetNumEdges() {
    GraphSizeType num_edges = 0;
    for (std::vector<GraphSizeType> &neighbors : adjacency_list_) {
      num_edges += neighbors.size();
    }
    return num_edges;
  }

  void PrintLayer(const std::vector<ScoreType> &layer, const std::vector<GraphSizeType> &order) {
    for (GraphSizeType i = 0; i < GetNumVertices(); ++i) {
      std::cerr << layer[order[i]] << " ";
    }
    std::cerr << std::endl;
  }

  void GenerateCompressedRepresentation() {
    GraphSizeType num_vertices = GetNumVertices();
    GraphSizeType num_edges = GetNumEdges();
    std::cerr << "# vertices: " << num_vertices << ", # edges: " << num_edges << std::endl;
    look_up_table_.reserve(GetNumVertices());
    look_up_table_.push_back(0);
    for (auto &neighbor_list : adjacency_list_) {
      GraphSizeType last_sum = look_up_table_.back();
      look_up_table_.push_back(neighbor_list.size());
      look_up_table_.back() += last_sum;
      neighbor_table_.insert(neighbor_table_.end(), neighbor_list.begin(), neighbor_list.end());
    }
  }

  void SetAlignmentParameters(const ScoreType substitution_penalty, const ScoreType deletion_penalty, const ScoreType insertion_penalty) {
    substitution_penalty_ = substitution_penalty;
    deletion_penalty_ = deletion_penalty;
    insertion_penalty_ = insertion_penalty;
  }

  void LoadFromTxtFile(const std::string &graph_file_path) {
    std::string line;
    std::ifstream infile(graph_file_path);
    GraphSizeType num_vertices;
    GraphSizeType row_index = 0;
    while (std::getline(infile, line)) {
      std::istringstream inputString(line);
      //get count of vertices from header row
        if (row_index == 0) {
          inputString >> num_vertices;
          compacted_graph_labels_.reserve(num_vertices);
        } else { //get out-neighbor vertex ids and vertex label
          assert(row_index <= num_vertices);
          compacted_graph_adjacency_list_.emplace_back(std::vector<GraphSizeType>());
          //Parse the input line
          std::vector<std::string> tokens (std::istream_iterator<std::string>{inputString}, std::istream_iterator<std::string>());
          assert(tokens.size() > 0);
          compacted_graph_labels_.emplace_back(tokens.back());
          for (auto it = tokens.begin(); it != tokens.end() && std::next(it) != tokens.end(); it++) {
            compacted_graph_adjacency_list_[row_index - 1].emplace_back(stoi(*it));
          }
        }
      row_index++;
    }
  }

  void OutputCompactedGraphInGFA(std::string &output_file_path) {
    std::ofstream outstrm(output_file_path);
    outstrm << "H\tVN:Z:1.0\n";
    for(uint32_t i = 0; i < compacted_graph_labels_.size(); ++i) {
      outstrm << "S\t" << i << "\t" << compacted_graph_labels_[i] << "\n";
    }
    for(uint32_t i = 0; i < compacted_graph_adjacency_list_.size(); ++i) {
      for (auto neighbor : compacted_graph_adjacency_list_[i]) {
        outstrm << "L\t" << i << "\t+\t" << neighbor << "\t+\t0M\n";
      }
    }
  }

  void GenerateCharLabeledGraph() {
    for (std::string &compacted_graph_label : compacted_graph_labels_) {
      labels_.emplace_back(compacted_graph_label[0]);
      adjacency_list_.emplace_back(std::vector<GraphSizeType>());
    }
    GraphSizeType vertex_id = compacted_graph_labels_.size(); // Keep the original vertex ids unchanged.
    GraphSizeType compacted_graph_vertex_id = 0;
    for (std::string &compacted_graph_label : compacted_graph_labels_) {
      GraphSizeType compacted_graph_label_length = compacted_graph_label.length();
      if (compacted_graph_label_length == 1) {
        adjacency_list_[compacted_graph_vertex_id].insert(adjacency_list_[compacted_graph_vertex_id].end(), compacted_graph_adjacency_list_[compacted_graph_vertex_id].begin(), compacted_graph_adjacency_list_[compacted_graph_vertex_id].end()); // add the neighbors of the chain to the neighbors of the last vertex
      }
      for (GraphSizeType i = 1; i < compacted_graph_label_length; ++i) {
        labels_.emplace_back(compacted_graph_label[i]);
        adjacency_list_.emplace_back(std::vector<GraphSizeType>());
        if (i == 1) { // the second vertex in the chain
          adjacency_list_[compacted_graph_vertex_id].push_back(vertex_id); // add the link from the first vertex to the second vertex in the chain
        } 
        if (i + 1 < compacted_graph_label_length) { // not the last vertex in the chain, which means it has a next vertex
          adjacency_list_[vertex_id].push_back(vertex_id + 1); // add link from current vertex to its next
        } else { // the last vertex in the chain
          adjacency_list_[vertex_id].insert(adjacency_list_[vertex_id].end(), compacted_graph_adjacency_list_[compacted_graph_vertex_id].begin(), compacted_graph_adjacency_list_[compacted_graph_vertex_id].end()); // add the neighbors of the chain to the neighbors of the last vertex
        }
        ++vertex_id;
      }
      ++compacted_graph_vertex_id;
    }
    // after the loop, compacted_graph_vertex_id should be the number of vertices in the compacted graph
    //assert(compacted_graph_vertex_id == GetNumVerticesInCompactedGraph());
    // add an edge between the dummy and every other vertex
    //adjacency_list_[0].reserve(vertex_id - 1);
    //for (GraphSizeType i = 1; i < vertex_id; ++i) {
    //  adjacency_list_[0].push_back(i);
    //}
  }

  void PropagateInsertions(const std::vector<ScoreType> &initialized_layer, const std::vector<GraphSizeType> &initialized_order, std::vector<ScoreType> *current_layer, std::vector<GraphSizeType> *current_order) {
    GraphSizeType num_vertices = GetNumVertices();
    GraphSizeType initialized_order_index = 0;
    GraphSizeType current_order_index = 0;
    visited_.assign(num_vertices, false);
    std::deque<GraphSizeType> updated_neighbors;
    while (initialized_order_index < num_vertices || !updated_neighbors.empty()) {
      GraphSizeType min_vertex = num_vertices;
      if (initialized_order_index < num_vertices && (updated_neighbors.empty() || (*current_layer)[initialized_order[initialized_order_index]] < (*current_layer)[updated_neighbors.front()])) {
        min_vertex = initialized_order[initialized_order_index];
        ++initialized_order_index;
      } else {
        min_vertex = updated_neighbors.front();
        updated_neighbors.pop_front();
      }
      if (!visited_[min_vertex]) {
        visited_[min_vertex] = true;
        (*current_order)[current_order_index] = min_vertex; 
        ++current_order_index;
        //for (auto neighbor : adjacency_list_[min_vertex]) {
        for (GraphSizeType neighbor_index = look_up_table_[min_vertex]; neighbor_index < look_up_table_[min_vertex + 1]; ++neighbor_index) {
          GraphSizeType neighbor = neighbor_table_[neighbor_index];
          if (!visited_[neighbor] && (*current_layer)[neighbor] > (*current_layer)[min_vertex] + insertion_penalty_) {
            (*current_layer)[neighbor] = (*current_layer)[min_vertex] + insertion_penalty_;
            updated_neighbors.push_back(neighbor);
          }
        }
      }
    }
  }
  
  void BuildOrderLookUpTable(const std::vector<ScoreType> &previous_layer, const std::vector<GraphSizeType> &previous_order){
    // Build the order look up table
    GraphSizeType num_vertices = GetNumVertices();
    GraphSizeType match_index = 0, substitution_index = 0, deletion_index = 0;
    GraphSizeType count = 0;
    while (count < 3 * num_vertices) {
      // Find the min
      ScoreType min_distance = previous_layer[previous_order[num_vertices - 1]] + substitution_penalty_ + deletion_penalty_ + 1;
      int min_type = -1; // 0 for match, 1 for substitution, 2 for deletion
      if (match_index < num_vertices && previous_layer[previous_order[match_index]] < min_distance) {
        min_distance = previous_layer[previous_order[match_index]];
        min_type = 0;
      }
      if (substitution_index < num_vertices && previous_layer[previous_order[substitution_index]] + substitution_penalty_ < min_distance) {
        min_distance = previous_layer[previous_order[substitution_index]] + substitution_penalty_;
        min_type = 1;
      } 
      if (deletion_index < num_vertices && previous_layer[previous_order[deletion_index]] + deletion_penalty_ < min_distance) {
        min_distance = previous_layer[previous_order[deletion_index]] + deletion_penalty_;
        min_type = 2;
      }
      // Put the order of min into the look up table
      if (min_type == 0) {
        order_look_up_table_[min_type * num_vertices + previous_order[match_index]] = count;
        ++match_index;
      } else if (min_type == 1) {
        order_look_up_table_[min_type * num_vertices + previous_order[substitution_index]] = count;
        ++substitution_index;
      } else {
        order_look_up_table_[min_type * num_vertices + previous_order[deletion_index]] = count;
        ++deletion_index;
      }
      ++count;
    }
  }

  void InitializeDistancesWithSorting(const char sequence_base, const std::vector<ScoreType> &previous_layer, const std::vector<GraphSizeType> &previous_order, std::vector<ScoreType> *initialized_layer, std::vector<GraphSizeType> *initialized_order) {
    GraphSizeType num_vertices = GetNumVertices();
    // Initialize the layer
    (*initialized_layer)[0] = previous_layer[0] + deletion_penalty_;
    for (GraphSizeType j = 1; j < num_vertices; ++j) {
      ScoreType cost = 0;
      if (sequence_base !=  labels_[j]) {
        cost = substitution_penalty_;
      }
      (*initialized_layer)[j] = previous_layer[0] + cost;
    }
    for (GraphSizeType i = 1; i < num_vertices; ++i) {
      if ((*initialized_layer)[i] > previous_layer[i] + deletion_penalty_) {
        (*initialized_layer)[i] = previous_layer[i] + deletion_penalty_;
      }
      //for (auto neighbor : adjacency_list_[i]) {
      for (GraphSizeType neighbor_index = look_up_table_[i]; neighbor_index < look_up_table_[i + 1]; ++neighbor_index) {
        GraphSizeType neighbor = neighbor_table_[neighbor_index];
        ScoreType cost = 0;
        if (sequence_base !=  labels_[neighbor]) {
          cost = substitution_penalty_;
        }
        if ((*initialized_layer)[neighbor] > previous_layer[i] + cost) {
          (*initialized_layer)[neighbor] = previous_layer[i] + cost;
        }
      }
    }
    // Use sorting to get order. 
    for (GraphSizeType vertex = 0; vertex < num_vertices; ++vertex) {
      distances_with_vertices_[vertex] = std::make_pair((*initialized_layer)[vertex], vertex);
    }
    std::sort(distances_with_vertices_.begin(), distances_with_vertices_.end());
    for (GraphSizeType i = 0; i < num_vertices; ++i) {
      (*initialized_order)[i] = distances_with_vertices_[i].second;
    }
  }

  void InitializeDistances(const char sequence_base, const std::vector<ScoreType> &previous_layer, const std::vector<GraphSizeType> &previous_order, std::vector<ScoreType> *initialized_layer, std::vector<GraphSizeType> *initialized_order) {
    GraphSizeType num_vertices = GetNumVertices();
    BuildOrderLookUpTable(previous_layer, previous_order);
    // Initialize the layer
    (*initialized_layer)[0] = previous_layer[0] + deletion_penalty_;
    parents_[0] = 0;
    types_[0] = 2;
    for (GraphSizeType j = 1; j < num_vertices; ++j) {
      ScoreType cost = 0;
      int type = 0;
      if (sequence_base !=  labels_[j]) {
        cost = substitution_penalty_;
        type = 1;
      }
      (*initialized_layer)[j] = previous_layer[0] + cost;
      parents_[j] = 0;
      types_[j] = type; 
    }
    for (GraphSizeType i = 1; i < num_vertices; ++i) {
      if ((*initialized_layer)[i] > previous_layer[i] + deletion_penalty_) {
        (*initialized_layer)[i] = previous_layer[i] + deletion_penalty_;
        parents_[i] = i;
        types_[i] = 2;
      }
      //for (auto neighbor : adjacency_list_[i]) {
      for (GraphSizeType neighbor_index = look_up_table_[i]; neighbor_index < look_up_table_[i + 1]; ++neighbor_index) {
        GraphSizeType neighbor = neighbor_table_[neighbor_index];
        ScoreType cost = 0;
        int type = 0;
        if (sequence_base !=  labels_[neighbor]) {
          cost = substitution_penalty_;
          type = 1;
        }
        if ((*initialized_layer)[neighbor] > previous_layer[i] + cost) {
          (*initialized_layer)[neighbor] = previous_layer[i] + cost;
          parents_[neighbor] = i;
          types_[neighbor] = type;
        }
      }
    }
    // Get the order. One should notice there can be multiple vertices share the same parent and type
    order_offsets_.assign(3 * num_vertices + 1, 0);
    order_counts_.assign(3 * num_vertices, 0);
    for (GraphSizeType i = 0; i < num_vertices; ++i) {
      order_offsets_[order_look_up_table_[types_[i] * num_vertices + parents_[i]] + 1]++;
    }
    for (GraphSizeType i = 1; i < 3 * num_vertices + 1; ++i) {
        order_offsets_[i] += order_offsets_[i - 1];
    }
    for (GraphSizeType i = 0; i < num_vertices; ++i) {
      GraphSizeType order = order_look_up_table_[types_[i] * num_vertices + parents_[i]];
      (*initialized_order)[order_offsets_[order] + order_counts_[order]] = i;
      ++order_counts_[order];
    }
  }

  ScoreType AlignUsingLinearGapPenalty(const sga::Sequence &sequence) {
    ScoreType max_cost = std::max(std::max(substitution_penalty_, deletion_penalty_), insertion_penalty_);
    GraphSizeType num_vertices = GetNumVertices();
    QueryLengthType sequence_length = sequence.GetLength();
    const std::string &sequence_bases = sequence.GetSequence();
    std::vector<ScoreType> previous_layer(num_vertices);
    std::vector<GraphSizeType> previous_order(num_vertices);
    std::vector<ScoreType> initialized_layer(num_vertices, sequence_length * max_cost + 1);
    std::vector<GraphSizeType> initialized_order(num_vertices);
    std::vector<ScoreType> current_layer(num_vertices, 0);
    std::vector<GraphSizeType> current_order;
    current_order.reserve(num_vertices); 
    for (GraphSizeType i = 0; i < num_vertices; ++i) {
      current_order.push_back(i);
    }
    order_look_up_table_.assign(3 * num_vertices, 0);
    parents_.assign(num_vertices, 0); 
    types_.assign(num_vertices, 0);
    //distances_with_vertices_.assign(num_vertices, std::make_pair(0,0));
    for (QueryLengthType i = 0; i < sequence_length; ++i) {
      std::swap(previous_layer, current_layer);
      std::swap(previous_order, current_order); 
      InitializeDistances(sequence_bases[i], previous_layer, previous_order, &initialized_layer, &initialized_order);
      //InitializeDistancesWithSorting(sequence_bases[i], previous_layer, previous_order, &initialized_layer, &initialized_order);
      current_layer = initialized_layer;
      PropagateInsertions(initialized_layer, initialized_order, &current_layer, &current_order); 
    }
    ScoreType forward_alignment_cost = current_layer[current_order[0]];
    // For reverse complement
    initialized_layer.assign(num_vertices, sequence_length * max_cost + 1);
    current_layer.assign(num_vertices, 0);
    for (GraphSizeType i = 0; i < num_vertices; ++i) {
      current_order[i] = i;
    }
    for (QueryLengthType i = 0; i < sequence_length; ++i) {
      std::swap(previous_layer, current_layer);
      std::swap(previous_order, current_order); 
      InitializeDistances(base_complement_[(int)sequence_bases[sequence_length - 1 - i]], previous_layer, previous_order, &initialized_layer, &initialized_order);
      //InitializeDistancesWithSorting(base_complement_[sequence_bases[sequence_length - 1 - i]], previous_layer, previous_order, &initialized_layer, &initialized_order);
      current_layer = initialized_layer;
      PropagateInsertions(initialized_layer, initialized_order, &current_layer, &current_order); 
    }
    ScoreType reverse_complement_alignment_cost = current_layer[current_order[0]];
    ScoreType min_alignment_cost = std::min(forward_alignment_cost, reverse_complement_alignment_cost);
    std::cerr << "Sequence length: " << sequence_length << ", forward alignment cost:" << forward_alignment_cost << ", reverse complement alignment cost:" << reverse_complement_alignment_cost << ", alignment cost:" << min_alignment_cost << std::endl;
    return min_alignment_cost;
  }

  void PropagateWithNavarroAlgorithm(const GraphSizeType from, const GraphSizeType to, GraphSizeType *num_propagations, std::vector<QueryLengthType> *current_layer) {
    (*num_propagations) += 1;
    if ((*current_layer)[to] > insertion_penalty_ + (*current_layer)[from]) {
      (*current_layer)[to] = insertion_penalty_ + (*current_layer)[from];
      for (GraphSizeType neighbor_index = look_up_table_[to]; neighbor_index < look_up_table_[to + 1]; ++neighbor_index) {
        GraphSizeType neighbor = neighbor_table_[neighbor_index];
        PropagateWithNavarroAlgorithm(to, neighbor, num_propagations, current_layer);
      }
    }
  }

  void ComputeLayerWithNavarroAlgorithm(const char sequence_base, const std::vector<QueryLengthType> &previous_layer, GraphSizeType *num_propagations, std::vector<QueryLengthType> *current_layer) {
    GraphSizeType num_vertices = GetNumVertices();
    // Initialize current layer
    (*current_layer)[0] = previous_layer[0] + deletion_penalty_;
    for (GraphSizeType j = 1; j < num_vertices; ++j) {
      QueryLengthType cost = 0;
      if (sequence_base !=  labels_[j]) {
        cost = substitution_penalty_;
      }
      (*current_layer)[j] = previous_layer[0] + cost;
    }
    for (GraphSizeType i = 1; i < num_vertices; ++i) {
      if ((*current_layer)[i] > previous_layer[i] + deletion_penalty_) {
        (*current_layer)[i] = previous_layer[i] + deletion_penalty_;
      }
      //for (auto neighbor : adjacency_list_[i]) {
      for (GraphSizeType neighbor_index = look_up_table_[i]; neighbor_index < look_up_table_[i + 1]; ++neighbor_index) {
        GraphSizeType neighbor = neighbor_table_[neighbor_index];
        QueryLengthType cost = 0;
        if (sequence_base !=  labels_[neighbor]) {
          cost = substitution_penalty_;
        }
        if ((*current_layer)[neighbor] > previous_layer[i] + cost) {
          (*current_layer)[neighbor] = previous_layer[i] + cost;
        }
      }
    }

    for (GraphSizeType i = 1; i < num_vertices; ++i) {
      for (GraphSizeType neighbor_index = look_up_table_[i]; neighbor_index < look_up_table_[i + 1]; ++neighbor_index) {
        GraphSizeType neighbor = neighbor_table_[neighbor_index];
        PropagateWithNavarroAlgorithm(i, neighbor, num_propagations, current_layer);
      }
    }
  }

  QueryLengthType AlignUsingLinearGapPenaltyWithNavarroAlgorithm(const sga::Sequence &sequence) {
    QueryLengthType max_cost = std::max(std::max(substitution_penalty_, deletion_penalty_), insertion_penalty_);
    GraphSizeType num_vertices = GetNumVertices();
    QueryLengthType sequence_length = sequence.GetLength();
    const std::string &sequence_bases = sequence.GetSequence();
    std::vector<QueryLengthType> previous_layer(num_vertices, sequence_length * max_cost + 1);
    std::vector<QueryLengthType> current_layer(num_vertices, 0);
    GraphSizeType num_propagations = 0;
    for (QueryLengthType i = 0; i < sequence_length; ++i) {
      std::swap(previous_layer, current_layer);
      ComputeLayerWithNavarroAlgorithm(sequence_bases[i], previous_layer, &num_propagations, &current_layer);
    }
    QueryLengthType forward_alignment_cost = *std::min_element(current_layer.begin(), current_layer.end());
    // For reverse complement
    previous_layer.assign(num_vertices, sequence_length * max_cost + 1);
    current_layer.assign(num_vertices, 0);
    for (QueryLengthType i = 0; i < sequence_length; ++i) {
      std::swap(previous_layer, current_layer);
      ComputeLayerWithNavarroAlgorithm(base_complement_[(int)sequence_bases[sequence_length - 1 - i]], previous_layer, &num_propagations, &current_layer);
    }
    QueryLengthType reverse_complement_alignment_cost = *std::min_element(current_layer.begin(), current_layer.end());
    QueryLengthType min_alignment_cost = std::min(forward_alignment_cost, reverse_complement_alignment_cost);
    std::cerr << "Sequence length: " << sequence_length << ", forward alignment cost:" << forward_alignment_cost << ", reverse complement alignment cost:" << reverse_complement_alignment_cost << ", alignment cost:" << min_alignment_cost << ", num propogations: " << num_propagations << std::endl;
    return min_alignment_cost;
  }

 protected:
  char base_complement_[256] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 'T', 4, 'G', 4, 4, 4, 'C', 4, 4, 4, 4, 4, 4, 'N', 4, 4, 4, 4, 4, 'A', 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 'T', 4, 'G', 4, 4, 4, 'C', 4, 4, 4, 4, 4, 4, 'N', 4, 4, 4, 4, 4, 'A', 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
  int8_t base_to_int_[256] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
  // For graph representation
  std::vector<GraphSizeType> look_up_table_;
  std::vector<GraphSizeType> neighbor_table_;
  std::vector<std::vector<GraphSizeType>> adjacency_list_;
  std::vector<char> labels_;
  std::vector<std::vector<GraphSizeType>> compacted_graph_adjacency_list_;
  std::vector<std::string> compacted_graph_labels_;
  // For RECOMB work
  std::vector<GraphSizeType> order_look_up_table_;
  std::vector<bool> visited_;
  std::vector<GraphSizeType> parents_;
  std::vector<int> types_;
  std::vector<GraphSizeType> order_offsets_;
  std::vector<GraphSizeType> order_counts_;
  // For alignment
  std::vector<std::pair<ScoreType, GraphSizeType> > distances_with_vertices_;
  ScoreType substitution_penalty_ = 1;
  ScoreType deletion_penalty_ = 1;
  ScoreType insertion_penalty_ = 1;
};

} // namespace sga
#endif // SGA_SEQUENCEGRAPH_H
