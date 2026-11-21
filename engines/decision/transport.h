#ifndef SAMOSA_DECISION_TRANSPORT_H
#define SAMOSA_DECISION_TRANSPORT_H
#include <functional>
#include <string>
#include <cstdint>
#include <vector>
namespace samosa::decision {
using RequestHandler=std::function<bool(const std::string&,std::string*,std::string*)>;
enum class TransportResult { Eof=0, InvalidFrame=1, IoFailure=2 };
// Repeated BE uint32 length + payload, bounded to 1 MiB in each direction.
// EOF between frames is clean shutdown. Caller must ignore SIGPIPE and owns
// cancellation/deadlines for the enclosing engine process. No fd is closed here.
TransportResult serve_requests(int input,int output,const RequestHandler&);
using ImageRequestHandler=std::function<bool(const std::vector<uint8_t>&,int,int,
    const std::string&,std::string*,std::string*)>;
// Explicit resident-images-v1 command: SMIMG001 + BE uint32 width/height/
// rgba_bytes/request_bytes, followed by RGBA then the unchanged typed JSON.
// Each axis is1..512; RGBA must be exactly w*h*4 (<=1MiB); JSON is1..1MiB.
// Responses keep the existing text frame. Partial/invalid image frames stop;
// complete typed refusals can recover on the next frame. No model is loaded.
TransportResult serve_image_requests(int input,int output,const ImageRequestHandler&);
}
#endif
