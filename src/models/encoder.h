#pragma once

#include "marian.h"
#include "models/states.h"

namespace marian {

class EncoderBase {
protected:
  Ptr<Options> options_;
  std::string prefix_{"encoder"};
  bool inference_{false};
  size_t batchIndex_{0};

  // @TODO: This used to be virtual, but is never overridden.
  // virtual

  Expr vmap(Expr chosenEmbeddings, Expr srcEmbeddings, const std::vector<size_t>& indices) const {
    static thread_local Ptr<std::unordered_map<size_t, size_t>> vmap;
    if(!options_->get<std::string>("vmap", "").empty()) {
      if(!vmap) {
        vmap = New<std::unordered_map<size_t, size_t>>();
        InputFileStream vmapFile(options_->get<std::string>("vmap"));
        size_t from, to;
        while(vmapFile >> from >> to) {
          (*vmap)[from] = to;
          std::cerr << from << " -> " << to << std::endl;
        }
      }
      else {
        std::vector<size_t> vmapped(indices.size());
        for(size_t i = 0; i < vmapped.size(); ++i) {
          if(vmap->count(i) > 0)
            vmapped[i] = (*vmap)[indices[i]];
          else
            vmapped[i] = i;
        }

        auto vmapEmbeddings = rows(srcEmbeddings, vmapped);
        chosenEmbeddings = (chosenEmbeddings + vmapEmbeddings) / 2.f;
      }
    }
    return chosenEmbeddings;
  }

  std::tuple<Expr, Expr> lookup(Ptr<ExpressionGraph> graph,
                                Expr srcEmbeddings,
                                Ptr<data::CorpusBatch> batch) const {
    using namespace keywords;

    auto subBatch = (*batch)[batchIndex_];

    int dimBatch = (int)subBatch->batchSize();
    int dimEmb = srcEmbeddings->shape()[-1];
    int dimWords = (int)subBatch->batchWidth();

    auto chosenEmbeddings = rows(srcEmbeddings, subBatch->data());
    chosenEmbeddings = vmap(chosenEmbeddings, srcEmbeddings, subBatch->data());

    auto batchEmbeddings
        = reshape(chosenEmbeddings, {dimWords, dimBatch, dimEmb});
    auto batchMask = graph->constant({dimWords, dimBatch, 1},
                                     inits::from_vector(subBatch->mask()));

    return std::make_tuple(batchEmbeddings, batchMask);
  }

public:
  EncoderBase(Ptr<Options> options)
      : options_(options),
        prefix_(options->get<std::string>("prefix", "encoder")),
        inference_(options->get<bool>("inference", false)),
        batchIndex_(options->get<size_t>("index", 0)) {}

  virtual Ptr<EncoderState> build(Ptr<ExpressionGraph>, Ptr<data::CorpusBatch>)
      = 0;

  template <typename T>
  T opt(const std::string& key) const {
    return options_->get<T>(key);
  }

  virtual void clear() = 0;
};

}  // namespace marian
