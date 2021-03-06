## Ristretto
Client/Server programs for online decoding using Kaldi's nnet3 decoder. Uses the fisher-english data set.
Both the client and server have Dockerfiles and there is a docker-compomse that mounts volumes on the current repository for quick changes. 
Client/Server communication is done with gRPC. As a note, this is nowhere near done.

------------------------
## Issues
- With gRPC 1.28, there was mysterious crashing on Stub destruction. Upgrading to 1.32 introduced errors with misaligned memory allocation
    - This blocks me from development as I was trying to get past this and then be done...
- Taking yet another long break from this

------------------------
## Setup
- Ristretto is divided into two containers: client and server
    - The client can easily be build/used natively but not the server
- These are available dockerhub: https://hub.docker.com/repository/docker/mik90/ristretto

### Client
- Can be built/used natively as well. I usually use a native build for testing with the microphone
    - Native builds require Conan
- Alpine image that takes in audio and sends it to the server
- Can read in audio files or take in audio from a microphone
    - Taking in audio from the mic via docker should be possible but I haven't tried it

### Server
- Based on the CUDA Ubuntu image and the Kaldi docker setup
    - Adds a newer GCC, cmake and gRPC
    - The image is really large ~20-25 GB
- I don't build this natively, I only use the container for development

------------------------
## TODO
- Chunk data on the client end
- Try out TLS/SSL
- The nnet3 API is made thread-safe by a single mutex at the start, not neat but hopefully it works
- Send audio chunks to Kaldi, what duration though?
    - currently reading in a whole audio file for debugging
- Logic for left context?
- Should this use a streaming RPC or a normal message based setup?
    - Start out as traditional setup but switch to streaming since audio can easily be streamed and the transcript may also
      be able to be streamed
- Optimize for response time
- Minimize the size of the server and client images
