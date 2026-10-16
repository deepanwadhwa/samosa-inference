#ifndef SAMOSA_DECISION_TRANSPORT_H
#define SAMOSA_DECISION_TRANSPORT_H
#include <functional>
#include <string>
namespace samosa::decision {
using RequestHandler=std::function<bool(const std::string&,std::string*,std::string*)>;
enum class TransportResult { Eof=0, InvalidFrame=1, IoFailure=2 };
// Repeated BE uint32 length + payload, bounded to 1 MiB in each direction.
// EOF between frames is clean shutdown. Caller must ignore SIGPIPE and owns
// cancellation/deadlines for the enclosing engine process. No fd is closed here.
TransportResult serve_requests(int input,int output,const RequestHandler&);
}
#endif
