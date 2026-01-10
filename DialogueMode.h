#pragma once

#include "WickedEngine.h"

#include <NsGui/Grid.h>
#include <NsGui/ScrollViewer.h>
#include <NsGui/StackPanel.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>

#include <string>

// Forward declaration
class NoesisRenderPath;

// Dialogue system for NPC conversations
class DialogueMode {
  public:
    DialogueMode() = default;
    ~DialogueMode() = default;

    // Initialize with UI elements from XAML
    void Initialize(Noesis::Grid *panelRoot, Noesis::ScrollViewer *scrollViewer,
                    Noesis::StackPanel *list, Noesis::TextBox *input, Noesis::TextBlock *hintText,
                    Noesis::FrameworkElement *talkIndicator);

    // Shutdown and release UI references
    void Shutdown();

    // Enter dialogue mode with an NPC
    void EnterDialogueMode(wi::ecs::Entity npcEntity, wi::scene::Scene &scene,
                           Noesis::IView *uiView);

    // Exit dialogue mode
    void ExitDialogueMode();

    // Clear all dialogue entries
    void ClearDialogue();

    // Add a dialogue entry to the panel
    void AddDialogueEntry(const std::string &speaker, const std::string &message);

    // Scroll dialogue to show newest content (at bottom)
    void ScrollDialogueToBottom();

    // Handle dialogue input submission
    void OnDialogueInputCommitted();

    // Check if dialogue mode is active
    bool IsActive() const { return inDialogueMode; }

    // Get the NPC entity being talked to
    wi::ecs::Entity GetDialogueNPCEntity() const { return dialogueNPCEntity; }

    // Get the NPC name for display
    const std::string &GetDialogueNPCName() const { return dialogueNPCName; }

    // Show/hide talk indicator
    void SetTalkIndicatorVisible(bool visible);

    // Callback setter for mode change notifications
    using ModeChangeCallback = std::function<void(bool entering)>;
    void SetModeChangeCallback(ModeChangeCallback callback) { modeChangeCallback = callback; }

  private:
    // UI elements
    Noesis::Ptr<Noesis::Grid> dialoguePanelRoot;
    Noesis::Ptr<Noesis::ScrollViewer> dialogueScrollViewer;
    Noesis::Ptr<Noesis::StackPanel> dialogueList;
    Noesis::Ptr<Noesis::TextBox> dialogueInput;
    Noesis::Ptr<Noesis::TextBlock> dialogueHintText;
    Noesis::Ptr<Noesis::FrameworkElement> talkIndicator;

    // State
    bool inDialogueMode = false;
    wi::ecs::Entity dialogueNPCEntity = wi::ecs::INVALID_ENTITY;
    std::string dialogueNPCName = "NPC";

    // Callback for mode changes
    ModeChangeCallback modeChangeCallback;
};
