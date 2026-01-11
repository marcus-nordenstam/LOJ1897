#include "DialogueMode.h"

#include <NsDrawing/Color.h>
#include <NsDrawing/Thickness.h>
#include <NsGui/Border.h>
#include <NsGui/Button.h>
#include <NsGui/Enums.h>
#include <NsGui/FontFamily.h>
#include <NsGui/FontProperties.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/TextProperties.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/RoutedEvent.h>

void DialogueMode::Initialize(Noesis::Grid *panelRoot, Noesis::ScrollViewer *scrollViewer,
                              Noesis::StackPanel *list, Noesis::TextBox *input,
                              Noesis::TextBlock *hintText, Noesis::FrameworkElement *indicator,
                              Noesis::StackPanel *recordInd, Noesis::Button *byeBtn) {
    dialoguePanelRoot = Noesis::Ptr<Noesis::Grid>(panelRoot);
    dialogueScrollViewer = Noesis::Ptr<Noesis::ScrollViewer>(scrollViewer);
    dialogueList = Noesis::Ptr<Noesis::StackPanel>(list);
    dialogueInput = Noesis::Ptr<Noesis::TextBox>(input);
    dialogueHintText = Noesis::Ptr<Noesis::TextBlock>(hintText);
    talkIndicator = Noesis::Ptr<Noesis::FrameworkElement>(indicator);
    recordIndicator = Noesis::Ptr<Noesis::StackPanel>(recordInd);
    byeButton = Noesis::Ptr<Noesis::Button>(byeBtn);
    
    // Set up click handler for Bye button
    if (byeButton) {
        byeButton->Click() += [this](Noesis::BaseComponent*, const Noesis::RoutedEventArgs&) {
            RequestExit();
        };
    }
}

void DialogueMode::Shutdown() {
    dialoguePanelRoot.Reset();
    dialogueScrollViewer.Reset();
    dialogueList.Reset();
    dialogueInput.Reset();
    dialogueHintText.Reset();
    talkIndicator.Reset();
    recordIndicator.Reset();
    byeButton.Reset();
    dialogueEntries.clear();
}

void DialogueMode::EnterDialogueMode(wi::ecs::Entity npcEntity, wi::scene::Scene &scene,
                                     Noesis::IView *uiView) {
    if (inDialogueMode)
        return;

    inDialogueMode = true;
    dialogueNPCEntity = npcEntity;

    // Get NPC name from scene
    const wi::scene::NameComponent *nameComp = scene.names.GetComponent(npcEntity);
    dialogueNPCName = nameComp ? nameComp->name : "NPC";

    // Clean up the name (remove path prefixes if present)
    size_t lastSlash = dialogueNPCName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        dialogueNPCName = dialogueNPCName.substr(lastSlash + 1);
    }

    // Show dialogue panel
    if (dialoguePanelRoot) {
        dialoguePanelRoot->SetVisibility(Noesis::Visibility_Visible);
    }

    // Hide talk indicator
    if (talkIndicator) {
        talkIndicator->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Clear previous dialogue and add initial greeting
    ClearDialogue();
    AddDialogueEntry("Dr Rabban", "Good day. How may I help you?");
    AddDialogueEntry("You", "Wohoo this is working");
    AddDialogueEntry("Dr Rabban", "We're not out of the woods just yet");

    // Focus the input box
    if (dialogueInput && uiView) {
        dialogueInput->Focus();
    }

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(true);
    }

    char buffer[256];
    sprintf_s(buffer, "Entered dialogue mode with %s (Entity: %llu)\n", dialogueNPCName.c_str(),
              npcEntity);
    wi::backlog::post(buffer);
}

void DialogueMode::ExitDialogueMode() {
    if (!inDialogueMode)
        return;

    inDialogueMode = false;
    dialogueNPCEntity = wi::ecs::INVALID_ENTITY;

    // Hide dialogue panel
    if (dialoguePanelRoot) {
        dialoguePanelRoot->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Hide hint text
    if (dialogueHintText) {
        dialogueHintText->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(false);
    }

    wi::backlog::post("Exited dialogue mode\n");
}

void DialogueMode::ClearDialogue() {
    if (dialogueList) {
        dialogueList->GetChildren()->Clear();
    }
    dialogueEntries.clear();
    hoveredEntryIndex = -1;
}

void DialogueMode::AddDialogueEntry(const std::string &speaker, const std::string &message) {
    if (!dialogueList)
        return;

    // Create TextBlock for the entry
    Noesis::Ptr<Noesis::TextBlock> textBlock = Noesis::MakePtr<Noesis::TextBlock>();

    // Format: "SPEAKER  -  Message"
    std::string entryText = speaker + "  -  " + message;
    textBlock->SetText(entryText.c_str());
    textBlock->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textBlock->SetFontSize(16.0f);

    // Override global TextBlock style properties that cause issues
    textBlock->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    textBlock->SetHorizontalAlignment(Noesis::HorizontalAlignment_Left);
    textBlock->SetFontWeight(Noesis::FontWeight_Normal);
    textBlock->SetEffect(nullptr); // Remove DropShadow from global style

    // Set font family
    Noesis::Ptr<Noesis::FontFamily> fontFamily =
        Noesis::MakePtr<Noesis::FontFamily>("Noesis/Data/Theme/Fonts/#PT Root UI");
    textBlock->SetFontFamily(fontFamily);

    // Set color based on speaker (player = cream, NPC = light blue)
    bool isPlayer = (speaker == "YOU" || speaker == "You");
    if (isPlayer) {
        Noesis::Ptr<Noesis::SolidColorBrush> brush =
            Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(235, 233, 225)); // #FFEBE9E1
        textBlock->SetForeground(brush);
    } else {
        Noesis::Ptr<Noesis::SolidColorBrush> brush =
            Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(154, 190, 214)); // #FF9ABED6
        textBlock->SetForeground(brush);
    }

    // Wrap in a Border for proper padding/margin
    Noesis::Ptr<Noesis::Border> border = Noesis::MakePtr<Noesis::Border>();
    border->SetPadding(Noesis::Thickness(14, 8, 14, 8));
    border->SetChild(textBlock);

    // Add to dialogue list
    dialogueList->GetChildren()->Add(border);

    // Track the entry
    DialogueEntry entry;
    entry.speaker = speaker;
    entry.message = message;
    entry.borderElement = border;
    entry.isPlayer = isPlayer;
    dialogueEntries.push_back(entry);

    // Scroll to bottom
    ScrollDialogueToBottom();
}

void DialogueMode::ScrollDialogueToBottom() {
    if (dialogueScrollViewer) {
        // Update layout first to ensure new content is measured
        dialogueScrollViewer->UpdateLayout();
        // Scroll to bottom to show newest messages
        dialogueScrollViewer->ScrollToEnd();
    }
}

void DialogueMode::OnDialogueInputCommitted() {
    if (!dialogueInput || !inDialogueMode)
        return;

    const char *inputText = dialogueInput->GetText();
    if (!inputText || strlen(inputText) == 0)
        return;

    // Add player's message
    AddDialogueEntry("YOU", inputText);

    // Clear input
    dialogueInput->SetText("");

    // Keep focus on input
    dialogueInput->Focus();

    // TODO: Process dialogue (AI response, game logic, etc.)
    // For now, add a placeholder NPC response
    // In the future, this would be replaced with actual dialogue system
}

void DialogueMode::SetTalkIndicatorVisible(bool visible) {
    if (talkIndicator) {
        talkIndicator->SetVisibility(visible ? Noesis::Visibility_Visible
                                             : Noesis::Visibility_Collapsed);
    }
}

void DialogueMode::UpdateDialogueHover(int mouseX, int mouseY) {
    // Clear previous hover highlight
    if (hoveredEntryIndex >= 0 && hoveredEntryIndex < (int)dialogueEntries.size()) {
        DialogueEntry& prevEntry = dialogueEntries[hoveredEntryIndex];
        if (prevEntry.borderElement) {
            prevEntry.borderElement->SetBackground(nullptr);
        }
    }
    
    hoveredEntryIndex = -1;

    if (!inDialogueMode || !dialogueList) {
        if (recordIndicator) {
            recordIndicator->SetVisibility(Noesis::Visibility_Collapsed);
        }
        return;
    }

    // Check each dialogue entry to see if mouse is over it
    for (size_t i = 0; i < dialogueEntries.size(); ++i) {
        DialogueEntry& entry = dialogueEntries[i];
        if (!entry.borderElement || entry.isPlayer) {
            continue; // Skip player messages
        }

        // Get the border's screen position and bounds
        Noesis::Point screenPos = entry.borderElement->PointToScreen(Noesis::Point(0, 0));
        float width = entry.borderElement->GetActualWidth();
        float height = entry.borderElement->GetActualHeight();

        // Check if mouse is over this element
        if (mouseX >= screenPos.x && mouseX <= screenPos.x + width &&
            mouseY >= screenPos.y && mouseY <= screenPos.y + height) {
            hoveredEntryIndex = (int)i;
            
            // Only show indicator and highlight for unrecorded messages
            if (!entry.isRecorded) {
                // Add yellow transparent background highlight
                Noesis::Ptr<Noesis::SolidColorBrush> highlightBrush =
                    Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(255, 255, 0, 60)); // Yellow with alpha
                entry.borderElement->SetBackground(highlightBrush);
                
                // Position and show record indicator
                if (recordIndicator) {
                    // Position to the left of the dialogue panel using margin
                    // This positions it in screen space relative to the dialogue entry
                    float indicatorX = screenPos.x - 200.0f;
                    float indicatorY = screenPos.y + (height / 2.0f) - 15.0f;
                    
                    recordIndicator->SetMargin(Noesis::Thickness(indicatorX, indicatorY, 0, 0));
                    recordIndicator->SetVisibility(Noesis::Visibility_Visible);
                }
            } else {
                // Already recorded - hide indicator
                if (recordIndicator) {
                    recordIndicator->SetVisibility(Noesis::Visibility_Collapsed);
                }
            }
            return;
        }
    }

    // No entry hovered, hide indicator
    if (recordIndicator) {
        recordIndicator->SetVisibility(Noesis::Visibility_Collapsed);
    }
}

const DialogueMode::DialogueEntry* DialogueMode::GetHoveredEntry() const {
    if (hoveredEntryIndex >= 0 && hoveredEntryIndex < (int)dialogueEntries.size()) {
        const DialogueEntry& entry = dialogueEntries[hoveredEntryIndex];
        if (!entry.isPlayer && !entry.isRecorded) {
            return &entry;
        }
    }
    return nullptr;
}

bool DialogueMode::IsRecordableMessageHovered() const {
    return GetHoveredEntry() != nullptr;
}

void DialogueMode::MarkHoveredAsRecorded() {
    if (hoveredEntryIndex >= 0 && hoveredEntryIndex < (int)dialogueEntries.size()) {
        DialogueEntry& entry = dialogueEntries[hoveredEntryIndex];
        entry.isRecorded = true;
        
        // Remove highlight
        if (entry.borderElement) {
            entry.borderElement->SetBackground(nullptr);
        }
        
        // Hide indicator
        if (recordIndicator) {
            recordIndicator->SetVisibility(Noesis::Visibility_Collapsed);
        }
    }
}

void DialogueMode::RequestExit() {
    if (exitRequestCallback) {
        exitRequestCallback();
    }
}
