#include "SpotifyMVDesktop.h"
#include <imgui.h>
#include <cfloat>
#include <string>

namespace {

const char* engine_state_label(Shared::VideoStreamEngine::State s) {
    switch (s) {
    case Shared::VideoStreamEngine::State::Idle:        return "Idle";
    case Shared::VideoStreamEngine::State::Downloading: return "Downloading";
    case Shared::VideoStreamEngine::State::Playing:     return "Playing";
    case Shared::VideoStreamEngine::State::Error:       return "Error";
    }
    return "Unknown";
}

ImVec4 engine_state_color(Shared::VideoStreamEngine::State s) {
    switch (s) {
    case Shared::VideoStreamEngine::State::Idle:        return {0.5f, 0.5f, 0.5f, 1};
    case Shared::VideoStreamEngine::State::Downloading: return {1, 1, 0, 1};
    case Shared::VideoStreamEngine::State::Playing:     return {0, 1, 0, 1};
    case Shared::VideoStreamEngine::State::Error:       return {1, 0, 0, 1};
    }
    return {1, 1, 1, 1};
}

void draw_error_text(const char* prefix, const std::string& error) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
    ImGui::TextWrapped("%s%s", prefix, error.c_str());
    ImGui::PopStyleColor();
}

void draw_engine_status(Shared::VideoStreamEngine::State state,
                         const std::string& error) {
    ImGui::TextColored(engine_state_color(state), "State: %s",
                       engine_state_label(state));
    ImGui::SameLine();
    ImGui::ProgressBar(0, ImVec2(-FLT_MIN, 0), "");
    if (state == Shared::VideoStreamEngine::State::Error && !error.empty())
        draw_error_text("", error);
}

} // anonymous namespace

void SpotifyMVDesktop::render() {
    ImGui::Text("Producer role: %s", producer_owner_.load(std::memory_order_acquire) ? "Active" : "Standby");
    if (!tools_available_.load()) {
        std::string error;
        {
            std::lock_guard<std::mutex> lock(tools_status_mutex_);
            error = tools_error_msg_;
        }
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", error.c_str());
        ImGui::TextDisabled("Configure the shared yt-dlp binary under External Tools.");
        return;
    }

    ImGui::SliderInt("Crossfade (ms)", &crossfade_duration_ms_, 0, 2000);

    {
        int chunk_sec = chunk_duration_sec_.load();
        if (ImGui::SliderInt("Chunk size (sec)", &chunk_sec, 5, 120)) {
            chunk_duration_sec_.store(chunk_sec);
        }
        ImGui::TextDisabled("Smaller chunks buffer less but stall less on slow connections.\n"
                             "Applies to the next track loaded, not the one currently playing.");
    }

    // ── Snapshot engine state under lock ────────────────────────────────
    std::string cur_url;
    Shared::VideoStreamEngine::State cur_state = Shared::VideoStreamEngine::State::Idle;
    std::string cur_error;
    bool has_pending = false;
    std::string pend_url;
    Shared::VideoStreamEngine::State pend_state = Shared::VideoStreamEngine::State::Idle;
    std::string pend_error;
    {
        std::lock_guard<std::mutex> lk(engine_mutex_);
        if (current_engine_) {
            cur_url = current_engine_->get_current_url();
            cur_state = current_engine_->get_state();
            cur_error = current_engine_->get_last_error();
        }
        if (pending_engine_) {
            has_pending = true;
            pend_url = pending_engine_->get_current_url();
            pend_state = pending_engine_->get_state();
            pend_error = pending_engine_->get_last_error();
        }
    }

    std::string cur_track;
    std::string pend_track;
    {
        std::lock_guard<std::mutex> lk(track_id_mutex_);
        cur_track = current_track_id_;
        pend_track = pending_track_id_;
    }

    // ── Overall status bar ──────────────────────────────────────────────
    {
        ImVec4 barColor = {0.3f, 0.3f, 0.3f, 1};
        const char* barLabel = "Idle";
        if (cur_state == Shared::VideoStreamEngine::State::Downloading) {
            barColor = {1, 1, 0, 1}; barLabel = "Downloading";
        } else if (cur_state == Shared::VideoStreamEngine::State::Playing) {
            barColor = {0, 1, 0, 1}; barLabel = "Playing";
        } else if (cur_state == Shared::VideoStreamEngine::State::Error) {
            barColor = {1, 0, 0, 1}; barLabel = "Error";
        }
        if (crossfade_active_.load()) {
            barColor = {0.5f, 0.5f, 1, 1}; barLabel = "Crossfading";
        } else if (search_running_.load() && cur_state != Shared::VideoStreamEngine::State::Playing) {
            barColor = {0, 0.84f, 0.38f, 1}; barLabel = "Searching";
        }
        ImGui::PushStyleColor(ImGuiCol_Button, barColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, barColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, barColor);
        ImGui::Button(barLabel, ImVec2(-FLT_MIN, 28));
        ImGui::PopStyleColor(3);
    }

    // ── Current track info ──────────────────────────────────────────────
    ImGui::Text("Track: %s", cur_track.empty() ? "None" : cur_track.c_str());
    if (!pend_track.empty())
        ImGui::Text("Next:  %s", pend_track.c_str());
    if (!cur_url.empty())
        ImGui::Text("URL:   %s", cur_url.c_str());

    // ── Search status ───────────────────────────────────────────────────
    if (search_running_.load()) {
        const char* preparation_label =
            cur_state == Shared::VideoStreamEngine::State::Playing
                ? "Preparing upcoming Spotify video in background..."
                : "Searching YouTube for Spotify video...";
        ImGui::TextColored(ImVec4(0, 0.84f, 0.38f, 1), "%s", preparation_label);
    }

    // ── Crossfade progress ──────────────────────────────────────────────
    if (crossfade_active_.load()) {
        auto elapsed = std::chrono::steady_clock::now() - crossfade_start_;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        float ratio = static_cast<float>(elapsed_ms) / crossfade_duration_ms_;
        float progress = ratio > 1.0f ? 1.0f : ratio;
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 0), "Crossfade");
    }

    // ── Engine status cards ─────────────────────────────────────────────
    if (ImGui::BeginTable("##engines", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Current Engine");
        draw_engine_status(cur_state, cur_error);
        ImGui::TableNextColumn();
        ImGui::SeparatorText("Pending Engine");
        if (has_pending)
            draw_engine_status(pend_state, pend_error);
        else
            ImGui::TextDisabled("None");
        ImGui::EndTable();
    }

    // ── Error display ────────────────────────────────────────────────────
    if (cur_state == Shared::VideoStreamEngine::State::Error && !cur_error.empty())
        draw_error_text("Error: ", cur_error);
    if (has_pending && pend_state == Shared::VideoStreamEngine::State::Error && !pend_error.empty())
        draw_error_text("Pending Error: ", pend_error);

    // ── Debug section ───────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Debug Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Total tracks played: %d", total_tracks_played_.load());
        ImGui::Text("Total errors:       %d", total_errors_.load());
        ImGui::Text("Engine swaps:       %d", total_swaps_.load());
        ImGui::Text("Background prepare: %s", search_running_.load() ? "Yes" : "No");
        ImGui::Text("Crossfade active:   %s", crossfade_active_.load() ? "Yes" : "No");
        ImGui::Separator();
        ImGui::Text("Current engine:     %s", current_engine_ ? "Valid" : "NULL");
        ImGui::Text("Pending engine:     %s", pending_engine_ ? "Valid" : "NULL");
        if (has_pending)
            ImGui::Text("Pending URL:  %s", pend_url.empty() ? "None" : pend_url.c_str());
    }
}
