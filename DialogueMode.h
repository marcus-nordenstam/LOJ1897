#pragma once

#include "GrymEngine.h"

#include <NsGui/Border.h>
#include <NsGui/Button.h>
#include <NsGui/Grid.h>
#include <NsGui/ScrollViewer.h>
#include <NsGui/StackPanel.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>

#include <string>
#include <vector>

// Forward declaration
class NoesisRenderPath;
class CaseboardMode;

// Dialogue system for NPC conversations
class DialogueMode {
  public:
    // Structure to track dialogue entries
    struct DialogueEntry {
        std::string speaker;
        std::string message;
        Noesis::Ptr<Noesis::Border> borderElement; // UI element container
        bool isPlayer = false;
        bool isRecorded = false; // True if this testimony has been recorded
    };

    DialogueMode() = default;
    ~DialogueMode() = default;

    // Initialize with UI elements from XAML
    void Initialize(Noesis::Grid *panelRoot, Noesis::ScrollViewer *scrollViewer,
                    Noesis::StackPanel *list, Noesis::TextBox *input, Noesis::TextBlock *hintText,
                    Noesis::FrameworkElement *talkIndicator, Noesis::StackPanel *recordIndicator,
                    Noesis::Button *byeButton);

    // Shutdown and release UI references
    void Shutdown();

    // Enter dialogue mode with an NPC
    void EnterDialogueMode(wi::ecs::Entity npcEntity, wi::scene::Scene &scene,
                           Noesis::IView *uiView);

    // Exit dialogue mode
    void ExitDialogueMode();

    // Request exit (triggers callback to NoesisRenderPath)
    void RequestExit();

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

    // Update hover state for dialogue entries (mouse position in screen space)
    void UpdateDialogueHover(int mouseX, int mouseY);

    // Get the hovered dialogue entry (returns nullptr if none hovered or if player message)
    const DialogueEntry *GetHoveredEntry() const;

    // Check if a recordable message is hovered (non-player message that hasn't been recorded)
    bool IsRecordableMessageHovered() const;

    // Mark the currently hovered entry as recorded
    void MarkHoveredAsRecorded();

    // Callback setter for mode change notifications
    using ModeChangeCallback = std::function<void(bool entering)>;
    void SetModeChangeCallback(ModeChangeCallback callback) { modeChangeCallback = callback; }

    // Callback setter for exit request
    using ExitRequestCallback = std::function<void()>;
    void SetExitRequestCallback(ExitRequestCallback callback) { exitRequestCallback = callback; }

  private:
    // UI elements
    Noesis::Ptr<Noesis::Grid> dialoguePanelRoot;
    Noesis::Ptr<Noesis::ScrollViewer> dialogueScrollViewer;
    Noesis::Ptr<Noesis::StackPanel> dialogueList;
    Noesis::Ptr<Noesis::TextBox> dialogueInput;
    Noesis::Ptr<Noesis::TextBlock> dialogueHintText;
    Noesis::Ptr<Noesis::FrameworkElement> talkIndicator;
    Noesis::Ptr<Noesis::StackPanel> recordIndicator; // "R - Record Testimony" indicator
    Noesis::Ptr<Noesis::Button> byeButton;           // Exit dialogue button

    // State
    bool inDialogueMode = false;
    wi::ecs::Entity dialogueNPCEntity = wi::ecs::INVALID_ENTITY;
    std::string dialogueNPCName = "NPC";

    // Dialogue entries tracking
    std::vector<DialogueEntry> dialogueEntries;
    int hoveredEntryIndex = -1;

    // Callback for mode changes
    ModeChangeCallback modeChangeCallback;
    ExitRequestCallback exitRequestCallback;
};
