/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2017, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "PCCCommon.h"

#ifdef USE_VTMLIB_VIDEO_CODEC
#include "PCCVideo.h"
#include "PCCVTMLibVideoEncoder.h"
#include "PCCVTMLibVideoEncoderImpl.h"
#include "PCCVTMLibVideoEncoderCfg.h"
#include "EncoderLib/EncLibCommon.h"
#include "../../../../dependencies/VTM/source/App/EncoderApp/EncApp.h"

#include <EncoderLib/PartitionManager.h>
#include <EncoderLib/PartitionPrediction.h>

//! \ingroup EncoderApp
//! \{

static const uint32_t settingNameWidth = 66;
static const uint32_t settingHelpWidth = 84;
static const uint32_t settingValueWidth = 3;
// --------------------------------------------------------------------------------------------------------------------- //
// Extern pointer to store and load the partition + current parameter
PartitionManager * store_partition;
PartitionManager * load_partition;
PartitionParam * param_partition;
PartitionPrediction * predict_partition;
PartitionPrediction * predict_partitionInter;



//fdeep::model *model = static_cast<fdeep::model *>(malloc(sizeof(fdeep::model)));
//std::unique_ptr<fdeep::model> model;

//string folder_model = "MODEL_DIRECTORY_HERE/cnn_model/" ;


float time_cnn = 0;

//macro value printing function

using namespace pcc;

template <typename T>
PCCVTMLibVideoEncoder<T>::PCCVTMLibVideoEncoder() {}
template <typename T>
PCCVTMLibVideoEncoder<T>::~PCCVTMLibVideoEncoder() {}

template <typename T>
void PCCVTMLibVideoEncoder<T>::encode( PCCVideo<T, 3>&            videoSrc,
                                       PCCVideoEncoderParameters& params,
                                       PCCVideoBitstream&         bitstream,
                                       PCCVideo<T, 3>&            videoRec ) {
  const size_t      width      = videoSrc.getWidth();
  const size_t      height     = videoSrc.getHeight();
  const size_t      frameCount = videoSrc.getFrameCount();
  std::stringstream cmd;
  cmd << "VTMEncoder";
  cmd << " -c " << params.encoderConfig_;
  cmd << " --InputFile=" << params.srcYuvFileName_;
  cmd << " --InputBitDepth=" << params.inputBitDepth_;
  cmd << " --InputChromaFormat=" << ( params.use444CodecIo_ ? "444" : "420" );
  cmd << " --OutputBitDepth=" << params.outputBitDepth_;
  cmd << " --OutputBitDepthC=" << params.outputBitDepth_;
  cmd << " --FrameRate=30";
  cmd << " --FrameSkip=0";
  cmd << " --SourceWidth=" << width;
  cmd << " --SourceHeight=" << height;
  cmd << " --ConformanceWindowMode=1 ";
  cmd << " --FramesToBeEncoded=" << frameCount;
  cmd << " --BitstreamFile=" << params.binFileName_;
  cmd << " --ReconFile=" << params.recYuvFileName_;
  cmd << " --QP=" << params.qp_;
  if ( params.internalBitDepth_ != 0 ) { cmd << " --InternalBitDepth=" << params.internalBitDepth_; }
  if ( params.usePccMotionEstimation_ ) {
    cmd << " --UsePccMotionEstimation=1"
        << " --BlockToPatchFile=" << params.blockToPatchFile_ << " --OccupancyMapFile=" << params.occupancyMapFile_
        << " --PatchInfoFile=" << params.patchInfoFile_;
  }
  if ( params.use444CodecIo_ ) { cmd << " --InputColourSpaceConvert=RGBtoGBR"; }
  std::cout << cmd.str() << std::endl;

  std::string arguments = cmd.str();

  fprintf( stdout, "\n" );
  fprintf( stdout, "VVCSoftware: VTM Encoder Version %s ", VTM_VERSION );
  fprintf( stdout, NVM_ONOS );
  fprintf( stdout, NVM_COMPILEDBY );
  fprintf( stdout, NVM_BITS );
  fprintf( stdout, "\n" );

  std::ostringstream oss( ostringstream::binary | ostringstream::out );
  std::ostream&      bitstreamFile = oss;
  EncLibCommon       encLibCommon;

  initROM();
  TComHash::initBlockSizeToIndex();

  PCCVTMLibVideoEncoderImpl<T> encoder( bitstreamFile, &encLibCommon );

  std::istringstream iss( arguments );
  std::string        token;
  std::vector<char*> args;
  while ( iss >> token ) {
    char* arg = new char[token.size() + 1];
    copy( token.begin(), token.end(), arg );
    arg[token.size()] = '\0';
    args.push_back( arg );
  }
  encoder.create();
  // parse configuration
  try {
    if ( !encoder.parseCfg( args.size(), &args[0] ) ) {
      encoder.destroy();
#if ENVIRONMENT_VARIABLE_DEBUG_AND_TEST
      EnvVar::printEnvVar();
#endif
      return;
    }
  } catch ( df::program_options_lite::ParseFailure& e ) {
    std::cerr << "Error parsing option \"" << e.arg << "\" with argument \"" << e.val << "\"." << std::endl;
    return;
  }
  for ( size_t i = 0; i < args.size(); i++ ) { delete[] args[i]; }

  encoder.createLib( 0 );
  if(0){
    encoder.set_predictPartition();
  }

  // Get partition param from config fileFF
  param_partition = new PartitionParam(encoder.getM_uiCTUSize(), 6*3/*Because I decided to use 6*3 bits to encode MTT in dat file*/, encoder.is_writePartition(), encoder.is_readPartition(), encoder.is_predictPartition(), encoder.is_predictPartitionInter());

  //sbelhadj added
  string folder_model = encoder.get_modelFolder() ;
  if (folder_model.substr(folder_model.length()-1) != "/") folder_model += "/" ;

  if(param_partition->is_writePartition()){
    //Get name of the input video to create dat file to save partition
    std::size_t posEnd = encoder.get_filenameInput().find_last_of("/");
    string filenameFeatures = encoder.get_filenameInput().substr(posEnd+1);
    std::size_t pos = filenameFeatures.find(".yuv");
    filenameFeatures = filenameFeatures.substr(0,pos);
    filenameFeatures += "_partition_" + to_string(encoder.get_qp()) + ".dat";
    filenameFeatures = encoder.get_datFolder() + "/" + filenameFeatures;
    // Create pointer to store partition
    store_partition = new PartitionManager(param_partition, (u_int16_t) encoder.get_sourceWidth(),
                                            (u_int16_t) encoder.get_sourceHeight(), filenameFeatures, !param_partition->is_writePartition());
    store_partition->store_params();
  }

  if(param_partition->is_readPartition()){
    //Get name of the input video to create dat file to save partition
    std::size_t posEnd = encoder.get_filenameInput().find_last_of("/");
    string filenameFeatures = encoder.get_filenameInput().substr(posEnd+1);
    std::size_t pos = filenameFeatures.find(".yuv");
    filenameFeatures = filenameFeatures.substr(0,pos);
    filenameFeatures += "_partition_" + to_string(encoder.get_qp()) + ".dat";
    filenameFeatures = encoder.get_datFolder() + "/" + filenameFeatures;
    // Create pointer to load partition
    load_partition = new PartitionManager(param_partition, (u_int16_t) encoder.get_sourceWidth(),
                                           (u_int16_t) encoder.get_sourceHeight(), filenameFeatures, param_partition->is_readPartition());
    load_partition->load_params();
  }

  // load the model if we predict intra partition
  if(param_partition->is_predictPartition() ){
    clock_t start = clock();
    //*model = fdeep::load_model(folder_model+"my_model_tech_db_filtered2020-05-13_15-05-43_0.094_0.094.json");

    predict_partition = new PartitionPrediction(folder_model+"cnn_model/intra/model.json", encoder.get_qp(), true);

    predict_partition->initializeModels("intra", folder_model);

    time_cnn += ((double) clock() - start) / CLOCKS_PER_SEC;
  }

  // load the model if we predict inter partition
  if(param_partition->is_predictPartitionInter() ){
    clock_t start = clock();
    // *model = fdeep::load_model(folder_model+"my_model_tech_db_filtered2020-05-13_15-05-43_0.094_0.094.json");

    //    predict_partitionInter = new PartitionPrediction(folder_model+"inter/20220228_145317_benchmark_mobileNetV2_filteredData.json", pcEncApp.at(0)->get_qp(), false);
    predict_partitionInter = new PartitionPrediction(folder_model+"cnn_model/inter/my_model_inter_3dim_mobilenetv2_batch256_100epoch_dbfilteredaugmented_gooddim_2021-04-19_15-31-43_0.04_0.044.json", encoder.get_qp(), false);

    predict_partitionInter->initializeModels("inter", folder_model);

    time_cnn += ((double) clock() - start) / CLOCKS_PER_SEC;
  }



  auto        startTime  = std::chrono::steady_clock::now();
  std::time_t startTime2 = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
  fprintf( stdout, " started @ %s", std::ctime( &startTime2 ) );
  clock_t startClock = clock();

  bool eos = false;

  while ( !eos ) {
    // read GOP
    bool keepLoop = true;
    while ( keepLoop ) {
#ifndef _DEBUG
      try {
#endif
        keepLoop = encoder.encodePrep( eos, videoSrc, arguments, videoRec );
#ifndef _DEBUG
      } catch ( Exception& e ) {
        std::cerr << e.what() << std::endl;
        return;
      } catch ( const std::bad_alloc& e ) {
        std::cout << "Memory allocation failed: " << e.what() << std::endl;
        return;
      }
#endif
    }

    // encode GOP
    keepLoop = true;
    while ( keepLoop ) {
#ifndef _DEBUG
      try {
#endif
        keepLoop = encoder.encode( videoSrc, arguments, bitstream, videoRec );
#ifndef _DEBUG
      } catch ( Exception& e ) {
        std::cerr << e.what() << std::endl;
        return;
      } catch ( const std::bad_alloc& e ) {
        std::cout << "Memory allocation failed: " << e.what() << std::endl;
        return;
      }
#endif
    }
  }

  auto buffer = oss.str();
  bitstream.resize( buffer.size() );
  std::copy( buffer.data(), buffer.data() + buffer.size(), bitstream.vector().begin() );

  clock_t     endClock = clock();
  auto        endTime  = std::chrono::steady_clock::now();
  std::time_t endTime2 = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
  auto        encTime  = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();

  printf( "\n finished @ %s", std::ctime( &endTime2 ) );
  printf( " Total Time: %12.3f sec. [user] %12.3f sec. [elapsed]\n", ( endClock - startClock ) * 1.0 / CLOCKS_PER_SEC,
          encTime / 1000.0 );

  encoder.destroyLib();
  encoder.destroy();
  destroyROM();

  printf( "\n finished @ %s", std::ctime( &endTime2 ) );

#if JVET_O0756_CALCULATE_HDRMETRICS
  printf( " Encoding Time (Total Time): %12.3f ( %12.3f ) sec. [user] %12.3f ( %12.3f ) sec. [elapsed]\n",
          ( ( endClock - startClock ) * 1.0 / CLOCKS_PER_SEC ) - ( metricTimeuser / 1000.0 ),
          ( endClock - startClock ) * 1.0 / CLOCKS_PER_SEC, encTime / 1000.0, totalTime / 1000.0 );
#else
  printf( " Total Time: %12.3f sec. [user] %12.3f sec. [elapsed]\n", ( endClock - startClock ) * 1.0 / CLOCKS_PER_SEC,
          encTime / 1000.0 );
#endif

  if(param_partition->is_predictPartition() || param_partition->is_predictPartitionInter()){
    std::cout<<"Time in the CNN + utilization of result: "<<time_cnn<<std::endl;
  }

  // Delete pointer
  delete param_partition;
  delete store_partition;
  delete load_partition;
  delete predict_partition;
  delete predict_partitionInter;


}

template class pcc::PCCVTMLibVideoEncoder<uint8_t>;
template class pcc::PCCVTMLibVideoEncoder<uint16_t>;

#endif
