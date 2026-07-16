#include "imgui_dialog.h"

#include <SFML/Window/Event.hpp>
#include <imgui-SFML.h>
#include <imgui.h>
#include <iostream>

namespace mgo {
namespace imgui {

Dialog::Dialog(sf::RenderWindow& window)
    : m_window(window)
{
    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Could not initialise Dialog-Sfml" << std::endl;
    }
    // Load a custom font BEFORE the first UpdateFontTexture call.
    // If we skip this, the default ImGui font is used automatically.
    ImGuiIO& io = ImGui::GetIO();
    // TODO io.Fonts->Clear();
    // TODO io.Fonts->AddFontFromFileTTF((dataDir + "/Oxanium-SemiBold.ttf").c_str(), 18.0f);
    auto _ = ImGui::SFML::UpdateFontTexture();

    // Disable creation of imgui.ini (enable by removing this line if you want
    // imgui windows to store their positions between runs)
    io.IniFilename = nullptr;

    // Enable gamepad input (need to call io.AddKeyEvent() with gamepad-mapped ImGuiKey_Gamepad*
    // values - it doesn't work automatically)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Colours
    // Easiest approach: pick a built-in style, then tweak individual colours.
    ImGui::StyleColorsDark(); // or StyleColorsLight(), StyleColorsClassic()

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.f, 0.3f, 0.0f, 0.45f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.f, 1.f, 0.0f, 0.45f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.f, 0.5f, 0.f, 1.f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.5f, 0.f, 1.f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.f, 0.9f, 0.0f, 1.f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.f, 0.45f, 0.f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.65f, 0.0f, 1.f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.f, 1.f, 0.f, 1.f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.f, 0.65f, 0.f, 1.f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.f, 0.65f, 0.f, 1.f);
    // The following affects all widgets, e.g. text input and checkboxes:
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.f, 0.25f, 0.f, 1.f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.f, 0.45f, 0.f, 1.f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.f, 0.45f, 0.f, 1.f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.f, 0.8f, 0.f, 0.5f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.f, 0.9f, 0.f, 0.5f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.f, 0.65f, 0.f, 1.f);
    // Selectable items
    style.Colors[ImGuiCol_Header] = ImVec4(0.f, 0.85f, 0.f, 1.00f); // selected item background
    style.Colors[ImGuiCol_HeaderHovered]
        = ImVec4(0.0f, 0.35f, 0.0f, 1.00f); // hovered item background
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.f, 0.65f, 0.f, 1.00f); // item being clicked
    style.WindowRounding = 8.0f;

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Always);
    ImGui::SetNextWindowPos({ 20.f, 200.f });
}

Dialog::~Dialog() noexcept
{
    ImGui::SFML::Shutdown();
}

std::vector<sf::Event> Dialog::processEvents()
{
    std::vector<sf::Event> pending;
    while (const std::optional event = m_window.pollEvent()) {
        if (!event.has_value()) {
            break;
        }
        ImGui::SFML::ProcessEvent(m_window, *event);
        pending.push_back(*event);
    }

    ImGui::SFML::Update(m_window, m_deltaClock.restart());
    const ImGuiIO& io = ImGui::GetIO();

    std::vector<sf::Event> unhandledEvents;
    for (const auto& ev : pending) {
        const bool isMouseEvent = ev.is<sf::Event::MouseButtonPressed>()
            || ev.is<sf::Event::MouseButtonReleased>() || ev.is<sf::Event::MouseMoved>()
            || ev.is<sf::Event::MouseWheelScrolled>();
        const bool isKeyEvent = ev.is<sf::Event::KeyPressed>() || ev.is<sf::Event::KeyReleased>()
            || ev.is<sf::Event::TextEntered>();

        if ((isMouseEvent && io.WantCaptureMouse) || (isKeyEvent && io.WantCaptureKeyboard)) {
            continue; // ImGui owns this one
        }
        unhandledEvents.push_back(ev);
    }
    return unhandledEvents;
}

Status Dialog::build()
{
    Status rc;
    // ImGui::SFML::Update(m_window, m_deltaClock.restart());
    ImGui::Begin("Main menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::Text("Options");
    ImGui::Separator();
    if (ImGui::Button("Breakable", ImVec2(120, 0))) {
        rc.breakable = true;
    }
    if (ImGui::Button("Edit", ImVec2(120, 0))) {
        rc.edit = true;
    }
    if (ImGui::Button("Line", ImVec2(120, 0))) {
        rc.line = true;
    }
    if (ImGui::Button("Fuel", ImVec2(120, 0))) {
        rc.fuel = true;
    }
    if (ImGui::Button("Start", ImVec2(120, 0))) {
        rc.start = true;
    }
    if (ImGui::Button("Exit", ImVec2(120, 0))) {
        rc.exit = true;
    }
    if (ImGui::Button("Moving", ImVec2(120, 0))) {
        rc.moving = true;
    }
    if (ImGui::Button("Polygon", ImVec2(120, 0))) {
        rc.polygon = true;
    }
    
    ImGui::Separator();
    if (ImGui::Button("Snap Mode", ImVec2(120, 0))) {
        rc.snapMode = true;
    }
    
    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(120, 0))) {
        rc.save = true;
    }
    if (ImGui::Button("Load", ImVec2(120, 0))) {
        rc.load = true;
    }

    ImGui::Separator();

    if (ImGui::Button("Quit", ImVec2(120, 0))) {
        rc.quit = true;
    }

    ImGui::End();
    return rc;
}

void Dialog::render()
{
    ImGui::SFML::Render(m_window);
}

} // namespace mgo
} // namespace imgui