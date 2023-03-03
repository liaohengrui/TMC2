//
// Created by liao on 23-2-28.
//

#include "PartitionGlobalManager.h"

PartitionManager * store_partition = nullptr;
PartitionManager * load_partition= nullptr;
PartitionParam * param_partition= nullptr;
PartitionPrediction * predict_partition= nullptr;
PartitionPrediction * predict_partitionInter= nullptr;
float time_cnn = 0;