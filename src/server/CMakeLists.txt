project(server)

find_package(kaldi CONFIG REQUIRED
    PATHS ${CMAKE_SOURCE_DIR}/../kaldi/dist/lib/cmake/kaldi
)

add_executable(RistrettoServer 
    main.cpp
    RistrettoServer.cpp
    ${ristretto_proto_srcs}
    ${ristretto_grpc_srcs}
)

# Turn spdlog onto the DEBUG level
target_compile_definitions(RistrettoServer PUBLIC SPDLOG_ACTIVE_LEVEL=1)

target_include_directories(RistrettoServer PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(RistrettoServer PRIVATE
    # Project settings
    project_options
	project_warnings

	# gRPC/Protobuf
    protobuf
    gRPC

    # Libs from conan
    CONAN_PKG::fmt
    CONAN_PKG::spdlog
    CONAN_PKG::boost

	# Kaldi and OpenFST
    fst
    kaldi-base
    kaldi-matrix
    kaldi-cudamatrix
    kaldi-util
    kaldi-feat
    kaldi-tree
    kaldi-gmm
    kaldi-transform
    kaldi-sgmm2
    kaldi-fstext
    kaldi-hmm
    kaldi-lm
    kaldi-decoder
    kaldi-lat
    kaldi-nnet
    kaldi-nnet2
    kaldi-nnet3
    kaldi-rnnlm
    kaldi-chain
    kaldi-ivector
    kaldi-online
    kaldi-online2
    kaldi-kws
    kaldi-cudafeat
    kaldi-cudadecoder
)