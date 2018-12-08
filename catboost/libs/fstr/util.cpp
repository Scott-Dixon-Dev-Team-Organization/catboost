#include "util.h"

#include <catboost/libs/algo/index_calcer.h>
#include <catboost/libs/helpers/exception.h>
#include <catboost/libs/target/data_providers.h>


using namespace NCB;


TVector<TVector<double>> CollectLeavesStatistics(
    const NCB::TDataProvider& dataset,
    const TFullModel& model,
    NPar::TLocalExecutor* localExecutor) {

    const auto* rawObjectsData = dynamic_cast<const TRawObjectsDataProvider*>(dataset.ObjectsData.Get());
    CB_ENSURE(rawObjectsData, "Quantized datasets are not supported yet");

    TRestorableFastRng64 rand(0);

    TProcessedDataProvider processedData = CreateModelCompatibleProcessedDataProvider(
        dataset,
        model,
        &rand,
        localExecutor);

    const size_t treeCount = model.ObliviousTrees.TreeSizes.size();
    TVector<TVector<double>> leavesStatistics(treeCount);
    for (size_t index = 0; index < treeCount; ++index) {
        leavesStatistics[index].resize(1 << model.ObliviousTrees.TreeSizes[index]);
    }

    auto binFeatures = BinarizeFeatures(model, *rawObjectsData);

    TConstArrayRef<float> weights = GetWeights(processedData.TargetData);

    const auto documentsCount = dataset.GetObjectCount();
    for (size_t treeIdx = 0; treeIdx < treeCount; ++treeIdx) {
        TVector<TIndexType> indices = BuildIndicesForBinTree(model, binFeatures, treeIdx);

        if (indices.empty()) {
            continue;
        }

        if (weights.empty()) {
            for (size_t doc = 0; doc < documentsCount; ++doc) {
                const TIndexType valueIndex = indices[doc];
                leavesStatistics[treeIdx][valueIndex] += 1.0;
            }
        } else {
            for (size_t doc = 0; doc < documentsCount; ++doc) {
                const TIndexType valueIndex = indices[doc];
                leavesStatistics[treeIdx][valueIndex] += weights[doc];
            }
        }
    }
    return leavesStatistics;
}

