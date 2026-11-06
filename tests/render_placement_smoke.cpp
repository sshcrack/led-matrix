#include "matrix_control/RenderPlacement.h"

#include <iostream>

namespace {
int failures = 0;
void expect(bool ok, const char *what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}
using Mode = RenderPlacement::Mode;
using NoFrame = RenderPlacement::NoRemoteFrame;
}

int main()
{
    {
        RenderPlacement p;
        expect(p.mode() == Mode::Local && !p.has_session(), "starts local");
        p.request_desktop(std::nullopt);
        expect(p.mode() == Mode::Local, "refused request stays local");
        p.request_desktop(7u);
        expect(p.mode() == Mode::DesktopPending && p.session() == 7u, "request enters pending");
        expect(p.no_remote_frame(std::nullopt) == NoFrame::RenderLocal, "pending renders locally");
        expect(p.remote_frame_presented(), "first remote frame goes live");
        expect(!p.remote_frame_presented(), "further frames are not a transition");
        expect(p.no_remote_frame(200.0) == NoFrame::HoldRemote && p.mode() == Mode::DesktopLive,
               "short stall holds the remote frame");
        expect(p.no_remote_frame(RenderPlacement::RemoteStallHoldMs + 1) == NoFrame::RenderLocal
                   && p.mode() == Mode::LocalFallback,
               "long stall falls back locally");
        expect(p.session() == 7u, "fallback keeps the session");
        expect(p.no_remote_frame(100.0) == NoFrame::RenderLocal, "fallback does not hold even if a frame looks fresh");
        expect(p.remote_frame_presented() && p.mode() == Mode::DesktopLive, "fresh frame resumes desktop");
        expect(p.worker_lost() && p.mode() == Mode::Local && !p.has_session(), "worker loss returns to local");
        expect(!p.worker_lost(), "worker loss in local is a no-op");
        expect(!p.remote_frame_presented(), "remote frames without a session are ignored");
    }
    {
        RenderPlacement p;
        const double budget = 10.0;
        for (unsigned i = 0; i + 1 < RenderPlacement::PressureFramesToOffload; ++i)
            expect(!p.local_frame_rendered(9.0, budget), "pressure below the streak does not offload");
        expect(p.local_frame_rendered(9.0, budget), "sustained pressure asks for the desktop");
        expect(!p.local_frame_rendered(9.0, budget), "streak restarts after a request");
        p.request_desktop(3u);
        for (int i = 0; i < 10; ++i)
            expect(!p.local_frame_rendered(9.0, budget), "pending placement never re-requests");
        p.worker_lost();
        expect(!p.local_frame_rendered(9.0, budget), "worker loss resets the streak");
    }
    if (failures == 0) std::cout << "RenderPlacement transitions OK\n";
    return failures == 0 ? 0 : 1;
}
