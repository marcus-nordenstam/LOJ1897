#pragma once

#include "NoesisRenderPath.h"

// Enter dialogue mode with an NPC
void NoesisRenderPath::EnterDialogueMode(wi::ecs::Entity npcEntity) {
    if (inDialogueMode)
        return;

    inDialogueMode = true;
    dialogueNPCEntity = npcEntity;

    // Get NPC name from scene
    wi::scene::Scene &scene = wi::scene::GetScene();
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

    // Hide aim dot
    aimDotVisible = false;

    // Release mouse capture for UI interaction
    SetThirdPersonMode(false);

    // Clear previous dialogue and add initial greeting
    ClearDialogue();
    AddDialogueEntry("Dr Rabban", "Good day. How may I help you?");
    AddDialogueEntry("You", "Wohoo this is working");
    AddDialogueEntry("Dr Rabban", "We're not out of the woods just yet");

    // Focus the input box
    if (dialogueInput && uiView) {
        dialogueInput->Focus();
    }

    char buffer[256];
    sprintf_s(buffer, "Entered dialogue mode with %s (Entity: %llu)\n", dialogueNPCName.c_str(),
              npcEntity);
    wi::backlog::post(buffer);
}

// Exit dialogue mode
void NoesisRenderPath::ExitDialogueMode() {
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

    // Re-enable walkabout controls
    SetThirdPersonMode(true);

    wi::backlog::post("Exited dialogue mode\n");
}

// Clear all dialogue entries
void NoesisRenderPath::ClearDialogue() {
    if (dialogueList) {
        dialogueList->GetChildren()->Clear();
    }
}

// Add a dialogue entry to the panel
void NoesisRenderPath::AddDialogueEntry(const std::string &speaker, const std::string &message) {
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

    // Scroll to bottom
    ScrollDialogueToBottom();
}

// Scroll dialogue to show newest content (at bottom)
void NoesisRenderPath::ScrollDialogueToBottom() {
    if (dialogueScrollViewer) {
        // Update layout first to ensure new content is measured
        dialogueScrollViewer->UpdateLayout();
        // Scroll to bottom to show newest messages
        dialogueScrollViewer->ScrollToEnd();
    }
}

// Handle dialogue input submission
void NoesisRenderPath::OnDialogueInputCommitted() {
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

// Enter caseboard mode
void NoesisRenderPath::EnterCaseboardMode() {
    if (inCaseboardMode || inDialogueMode)
        return;

    inCaseboardMode = true;
    caseboardPanning = false;

    // Calculate visible area dimensions based on window aspect ratio
    if (windowHandle) {
        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        float viewportHoriz = (float)(clientRect.right - clientRect.left);
        float viewportVert = (float)(clientRect.bottom - clientRect.top);

        // Set visible area: 3000 units in the smaller dimension, scaled for aspect ratio
        // This ensures the visible area matches the window's aspect ratio
        if (viewportHoriz > viewportVert) {
            // Landscape: height is smaller
            caseboardVisibleHalfY = 3000.0f;
            caseboardVisibleHalfX = 3000.0f * (viewportHoriz / viewportVert);
        } else {
            // Portrait: width is smaller
            caseboardVisibleHalfX = 3000.0f;
            caseboardVisibleHalfY = 3000.0f * (viewportVert / viewportHoriz);
        }

        char buf[256];
        sprintf_s(buf, "Caseboard visible area: %.0f x %.0f (half-extents)\n",
                  caseboardVisibleHalfX, caseboardVisibleHalfY);
        wi::backlog::post(buf);
    }

    // Reset pan/zoom to default (origin centered on screen)
    caseboardZoom = 1.0f;
    // Center origin on screen: pan = viewport / 2
    if (windowHandle) {
        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        caseboardPanX = (float)(clientRect.right - clientRect.left) / 2.0f;
        caseboardPanY = (float)(clientRect.bottom - clientRect.top) / 2.0f;
    } else {
        caseboardPanX = 0.0f;
        caseboardPanY = 0.0f;
    }
    UpdateCaseboardTransforms();

    // Show caseboard panel
    if (caseboardPanel) {
        caseboardPanel->SetVisibility(Noesis::Visibility_Visible);
    }

    // Hide talk indicator
    if (talkIndicator) {
        talkIndicator->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Hide aim dot
    aimDotVisible = false;

    // Release mouse capture for UI interaction
    SetThirdPersonMode(false);

    wi::backlog::post("Entered caseboard mode\n");
}

// Exit caseboard mode
void NoesisRenderPath::ExitCaseboardMode() {
    if (!inCaseboardMode)
        return;

    // Finalize any note card being edited
    if (editingNoteCardIndex >= 0) {
        FinalizeNoteCardEdit();
    }

    inCaseboardMode = false;
    caseboardPanning = false;

    // Hide caseboard panel
    if (caseboardPanel) {
        caseboardPanel->SetVisibility(Noesis::Visibility_Collapsed);
    }

    // Re-enable walkabout controls
    SetThirdPersonMode(true);

    wi::backlog::post("Exited caseboard mode\n");
}

// Update caseboard transforms based on current pan/zoom state
void NoesisRenderPath::UpdateCaseboardTransforms() {
    if (caseboardZoomTransform) {
        caseboardZoomTransform->SetScaleX(caseboardZoom);
        caseboardZoomTransform->SetScaleY(caseboardZoom);
    }
    if (caseboardPanTransform) {
        caseboardPanTransform->SetX(caseboardPanX);
        caseboardPanTransform->SetY(caseboardPanY);
    }
    UpdateCaseboardDebugText();
}

// Update caseboard debug text with current mouse position (board space) and zoom
void NoesisRenderPath::UpdateCaseboardDebugText() {
    if (!caseboardDebugText)
        return;

    // Convert screen mouse position to board space
    // Board space = (screen - pan) / zoom
    float boardX = (caseboardCurrentMousePos.x - caseboardPanX) / caseboardZoom;
    float boardY = (caseboardCurrentMousePos.y - caseboardPanY) / caseboardZoom;

    // Format debug text showing position and visible area bounds
    char debugStr[256];
    snprintf(debugStr, sizeof(debugStr),
             "Board: (%.0f, %.0f)  Zoom: %.2fx  Visible: +/-%.0f x +/-%.0f", boardX, boardY,
             caseboardZoom, caseboardVisibleHalfX, caseboardVisibleHalfY);
    caseboardDebugText->SetText(debugStr);
}

// Handle caseboard zoom (mouse wheel)
void NoesisRenderPath::CaseboardZoom(int x, int y, float delta) {
    if (!inCaseboardMode)
        return;

    // Store mouse position before zoom
    float mouseX = (float)x;
    float mouseY = (float)y;

    // Calculate world position under mouse before zoom
    float worldXBefore = (mouseX - caseboardPanX) / caseboardZoom;
    float worldYBefore = (mouseY - caseboardPanY) / caseboardZoom;

    // Apply zoom
    float zoomDelta = delta * 0.001f; // Adjust sensitivity
    caseboardZoom = std::clamp(caseboardZoom + zoomDelta, 0.2f, 4.0f);

    // Calculate world position under mouse after zoom
    float worldXAfter = (mouseX - caseboardPanX) / caseboardZoom;
    float worldYAfter = (mouseY - caseboardPanY) / caseboardZoom;

    // Adjust pan to keep mouse position stable
    caseboardPanX += (worldXAfter - worldXBefore) * caseboardZoom;
    caseboardPanY += (worldYAfter - worldYBefore) * caseboardZoom;

    // Clamp pan to board bounds after zoom
    ClampCaseboardPan();

    UpdateCaseboardTransforms();
}

// Handle caseboard pan start (mouse down)
void NoesisRenderPath::CaseboardPanStart(int x, int y) {
    if (!inCaseboardMode)
        return;

    // Convert click position to board space
    float boardClickX = (x - caseboardPanX) / caseboardZoom;
    float boardClickY = (y - caseboardPanY) / caseboardZoom;

    // If we're editing a note and click elsewhere, finalize the edit
    if (editingNoteCardIndex >= 0 && editingNoteCardIndex < (int)noteCards.size()) {
        NoteCard &card = noteCards[editingNoteCardIndex];

        // Check if click is outside the note card
        float cardLeft = card.boardX - 90.0f;
        float cardTop = card.boardY - 110.0f;
        float cardRight = cardLeft + 180.0f;
        float cardBottom = cardTop + 220.0f;

        bool clickedOutside = (boardClickX < cardLeft || boardClickX > cardRight ||
                               boardClickY < cardTop || boardClickY > cardBottom);

        if (clickedOutside) {
            FinalizeNoteCardEdit();
        }
    }

    // Check if clicking on a non-editing note card to start editing it
    if (editingNoteCardIndex < 0) {
        for (int i = 0; i < (int)noteCards.size(); i++) {
            NoteCard &card = noteCards[i];
            if (card.isEditing)
                continue; // Already being edited (shouldn't happen)

            // Check if click is inside this note card's editable area
            float cardLeft = card.boardX - 90.0f + 15.0f; // Add text margin
            float cardTop = card.boardY - 110.0f + 40.0f; // Add text margin
            float cardRight = card.boardX - 90.0f + 180.0f - 15.0f;
            float cardBottom = card.boardY - 110.0f + 220.0f - 20.0f;

            if (boardClickX >= cardLeft && boardClickX <= cardRight && boardClickY >= cardTop &&
                boardClickY <= cardBottom) {
                // Start editing this note card
                StartEditingNoteCard(i);
                return; // Don't start panning
            }
        }
    }

    // Check if clicking on a note card's drag area (outside text area)
    int dragCardIndex = HitTestNoteCardDragArea(boardClickX, boardClickY);
    if (dragCardIndex >= 0) {
        StartDraggingNoteCard(dragCardIndex, boardClickX, boardClickY);
        return; // Don't start panning
    }

    caseboardPanning = true;
    caseboardLastMousePos.x = x;
    caseboardLastMousePos.y = y;
}

// Handle caseboard pan end (mouse up)
void NoesisRenderPath::CaseboardPanEnd() {
    caseboardPanning = false;
    StopDraggingNoteCard();
}

// Handle caseboard pan move (mouse move while dragging)
// Clamp pan values to keep board visible within viewport
void NoesisRenderPath::ClampCaseboardPan() {
    if (!windowHandle)
        return;

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);

    // Actual viewport dimensions
    float viewportHoriz = (float)(clientRect.right - clientRect.left);
    float viewportVert = (float)(clientRect.bottom - clientRect.top);

    // Pan limits based on visible area (symmetric around origin: -halfX to +halfX)
    // To see board position -halfX at screen left: 0 = -halfX * zoom + pan → pan = halfX * zoom
    // To see board position +halfX at screen right: viewport = halfX * zoom + pan → pan =
    // viewport - halfX * zoom
    float maxPanX = caseboardVisibleHalfX * caseboardZoom;
    float minPanX = viewportHoriz - caseboardVisibleHalfX * caseboardZoom;

    float maxPanY = caseboardVisibleHalfY * caseboardZoom;
    float minPanY = viewportVert - caseboardVisibleHalfY * caseboardZoom;

    // If visible area is smaller than viewport at current zoom, center origin on screen
    if (minPanX > maxPanX) {
        caseboardPanX = viewportHoriz / 2.0f;
    } else {
        caseboardPanX = std::clamp(caseboardPanX, minPanX, maxPanX);
    }

    if (minPanY > maxPanY) {
        caseboardPanY = viewportVert / 2.0f;
    } else {
        caseboardPanY = std::clamp(caseboardPanY, minPanY, maxPanY);
    }
}

void NoesisRenderPath::CaseboardPanMove(int x, int y) {
    if (!inCaseboardMode)
        return;

    // Always update current mouse pos for debug display
    caseboardCurrentMousePos.x = x;
    caseboardCurrentMousePos.y = y;

    // Convert to board space for drag/hover detection
    float boardX = (x - caseboardPanX) / caseboardZoom;
    float boardY = (y - caseboardPanY) / caseboardZoom;

    // Handle note card dragging
    if (IsDraggingNoteCard()) {
        UpdateDraggingNoteCard(boardX, boardY);
        UpdateCaseboardDebugText();
        return;
    }

    if (caseboardPanning) {
        float deltaX = (float)(x - caseboardLastMousePos.x);
        float deltaY = (float)(y - caseboardLastMousePos.y);

        caseboardPanX += deltaX;
        caseboardPanY += deltaY;

        // Clamp pan to board bounds
        ClampCaseboardPan();

        caseboardLastMousePos.x = x;
        caseboardLastMousePos.y = y;

        UpdateCaseboardTransforms();
    } else {
        // Check if hovering over a note card drag area for cursor change
        int hoverCardIndex = HitTestNoteCardDragArea(boardX, boardY);
        if (hoverCardIndex >= 0 && windowHandle) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
        }

        // Just update debug text when not panning
        UpdateCaseboardDebugText();
    }
}

// Add a note card at the center of the current view
void NoesisRenderPath::AddNoteCard() {
    if (!inCaseboardMode || !caseboardContent)
        return;

    // Get the center of the current view in board space
    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    float viewCenterX = (float)(clientRect.right - clientRect.left) / 2.0f;
    float viewCenterY = (float)(clientRect.bottom - clientRect.top) / 2.0f;

    // Convert to board space
    float boardX = (viewCenterX - caseboardPanX) / caseboardZoom;
    float boardY = (viewCenterY - caseboardPanY) / caseboardZoom;

    // Create the note card container (Grid with Image background and TextBox)
    Noesis::Ptr<Noesis::Grid> noteContainer = *new Noesis::Grid();
    noteContainer->SetWidth(180.0f);
    noteContainer->SetHeight(220.0f);

    // Create and set up the background image
    Noesis::Ptr<Noesis::Image> bgImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> bitmapSource = *new Noesis::BitmapImage();
    bitmapSource->SetUriSource(Noesis::Uri("GUI/Cards/Note_Card.png"));
    bgImage->SetSource(bitmapSource);
    bgImage->SetStretch(Noesis::Stretch_Fill);
    noteContainer->GetChildren()->Add(bgImage);

    // Create the text box for note input
    Noesis::Ptr<Noesis::TextBox> textBox = *new Noesis::TextBox();
    textBox->SetText("");
    textBox->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textBox->SetAcceptsReturn(true);
    textBox->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    textBox->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    textBox->SetMargin(
        Noesis::Thickness(15.0f, 20.0f, 15.0f, 20.0f)); // Padding for note paper lines
    textBox->SetFontSize(16.0f);
    textBox->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));

    // Dark blue ink color for note text
    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textBox->SetForeground(inkBrush);
    textBox->SetBackground(nullptr);                      // Transparent background
    textBox->SetBorderThickness(Noesis::Thickness(0.0f)); // No border
    textBox->SetCaretBrush(inkBrush);
    noteContainer->GetChildren()->Add(textBox);

    // Position the note card on the canvas
    Noesis::Canvas::SetLeft(noteContainer, boardX - 90.0f); // Center horizontally
    Noesis::Canvas::SetTop(noteContainer, boardY - 110.0f); // Center vertically

    // Add to the caseboard content
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(noteContainer);
    }

    // Store reference
    NoteCard card;
    card.container = noteContainer;
    card.textBox = textBox;
    card.textLabel = nullptr; // Will be created when editing is finalized
    card.boardX = boardX;
    card.boardY = boardY;
    card.isEditing = true;
    card.savedText = "";
    noteCards.push_back(card);

    // Track which note card is being edited
    editingNoteCardIndex = (int)noteCards.size() - 1;

    // Focus the text box for immediate typing
    textBox->Focus();

    char buf[128];
    sprintf_s(buf, "Added note card at board position (%.0f, %.0f)\n", boardX, boardY);
    wi::backlog::post(buf);
}

// Finalize note card editing - save text and replace TextBox with TextBlock
void NoesisRenderPath::FinalizeNoteCardEdit() {
    if (editingNoteCardIndex < 0 || editingNoteCardIndex >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[editingNoteCardIndex];
    if (!card.isEditing || !card.textBox)
        return;

    // Get the text from the TextBox
    const char *text = card.textBox->GetText();
    card.savedText = text ? text : "";

    // Remove the TextBox from the container
    Noesis::UIElementCollection *children = card.container->GetChildren();
    if (children) {
        children->Remove(card.textBox.GetPtr());
    }

    // Create a TextBlock to display the saved text
    Noesis::Ptr<Noesis::TextBlock> textLabel = *new Noesis::TextBlock();
    textLabel->SetText(card.savedText.c_str());
    textLabel->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    textLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Left);
    // Adjusted margins to match TextBox internal padding
    textLabel->SetMargin(Noesis::Thickness(27.0f, 28.0f, 15.0f, 20.0f));
    textLabel->SetFontSize(16.0f);
    textLabel->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));
    textLabel->SetFontWeight(Noesis::FontWeight_Normal);

    // Dark blue ink color
    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textLabel->SetForeground(inkBrush);

    // Override the global TextBlock style effects
    textLabel->SetEffect(nullptr);

    // Add the TextBlock to the container
    if (children) {
        children->Add(textLabel);
    }

    // Update the note card state
    card.textLabel = textLabel;
    card.textBox.Reset(); // Release the TextBox
    card.isEditing = false;

    editingNoteCardIndex = -1;

    wi::backlog::post("Finalized note card edit\n");
}

// Handle click on caseboard - finalize any active note edit
void NoesisRenderPath::OnCaseboardClick() {
    if (editingNoteCardIndex >= 0) {
        FinalizeNoteCardEdit();
    }
}

// Start editing an existing note card (replace TextBlock with TextBox)
void NoesisRenderPath::StartEditingNoteCard(int index) {
    if (index < 0 || index >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[index];
    if (card.isEditing)
        return; // Already editing

    // Remove the TextBlock from the container
    Noesis::UIElementCollection *children = card.container->GetChildren();
    if (children && card.textLabel) {
        children->Remove(card.textLabel.GetPtr());
    }

    // Create a new TextBox with the saved text
    Noesis::Ptr<Noesis::TextBox> textBox = *new Noesis::TextBox();
    textBox->SetText(card.savedText.c_str());
    textBox->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textBox->SetAcceptsReturn(true);
    textBox->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    textBox->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    textBox->SetMargin(Noesis::Thickness(15.0f, 20.0f, 15.0f, 20.0f));
    textBox->SetFontSize(16.0f);
    textBox->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));

    // Dark blue ink color
    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textBox->SetForeground(inkBrush);
    textBox->SetBackground(nullptr);
    textBox->SetBorderThickness(Noesis::Thickness(0.0f));
    textBox->SetCaretBrush(inkBrush);

    // Add the TextBox to the container
    if (children) {
        children->Add(textBox);
    }

    // Update state
    card.textBox = textBox;
    card.textLabel.Reset();
    card.isEditing = true;
    editingNoteCardIndex = index;

    // Focus the text box
    textBox->Focus();

    wi::backlog::post("Started editing existing note card\n");
}

// Check if a board position is on a note card's draggable area (not the text area)
// Returns the index of the note card, or -1 if not on any
int NoesisRenderPath::HitTestNoteCardDragArea(float boardX, float boardY) {
    for (int i = (int)noteCards.size() - 1; i >= 0; i--) { // Check top cards first
        NoteCard &card = noteCards[i];

        // Full card bounds
        float cardLeft = card.boardX - 90.0f;
        float cardTop = card.boardY - 110.0f;
        float cardRight = cardLeft + 180.0f;
        float cardBottom = cardTop + 220.0f;

        // Text/editable area bounds
        float textLeft = cardLeft + 15.0f;
        float textTop = cardTop + 20.0f;
        float textRight = cardRight - 15.0f;
        float textBottom = cardBottom - 20.0f;

        // Check if inside card
        if (boardX >= cardLeft && boardX <= cardRight && boardY >= cardTop &&
            boardY <= cardBottom) {
            // Check if NOT in text area (draggable area is outside text area)
            bool inTextArea = (boardX >= textLeft && boardX <= textRight && boardY >= textTop &&
                               boardY <= textBottom);
            if (!inTextArea) {
                return i;
            }
        }
    }
    return -1;
}

// Start dragging a note card
void NoesisRenderPath::StartDraggingNoteCard(int index, float boardX, float boardY) {
    if (index < 0 || index >= (int)noteCards.size())
        return;

    // If editing this card, finalize first
    if (editingNoteCardIndex == index) {
        FinalizeNoteCardEdit();
    }

    draggingNoteCardIndex = index;
    NoteCard &card = noteCards[index];

    // Store offset from card center to mouse position
    dragOffsetX = boardX - card.boardX;
    dragOffsetY = boardY - card.boardY;

    wi::backlog::post("Started dragging note card\n");
}

// Update dragged note card position
void NoesisRenderPath::UpdateDraggingNoteCard(float boardX, float boardY) {
    if (draggingNoteCardIndex < 0 || draggingNoteCardIndex >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[draggingNoteCardIndex];

    // Update position (accounting for drag offset)
    card.boardX = boardX - dragOffsetX;
    card.boardY = boardY - dragOffsetY;

    // Update the canvas position
    if (card.container) {
        Noesis::Canvas::SetLeft(card.container, card.boardX - 90.0f);
        Noesis::Canvas::SetTop(card.container, card.boardY - 110.0f);
    }
}

// Stop dragging
void NoesisRenderPath::StopDraggingNoteCard() {
    if (draggingNoteCardIndex >= 0) {
        wi::backlog::post("Stopped dragging note card\n");
    }
    draggingNoteCardIndex = -1;
}

// Enter camera mode
void NoesisRenderPath::EnterCameraMode() {
    if (inCameraMode || inDialogueMode || inCaseboardMode || inMainMenuMode)
        return;

    inCameraMode = true;

    // Switch to first-person camera
    SetFirstPersonMode(true);

    // Hide player character by setting SetRenderable(false) on all object components
    hiddenPlayerObjects.clear();
    if (playerCharacter != wi::ecs::INVALID_ENTITY) {
        wi::scene::Scene &scene = wi::scene::GetScene();

        // Hide the player's own object component if it has one
        wi::scene::ObjectComponent *playerObject = scene.objects.GetComponent(playerCharacter);
        if (playerObject) {
            playerObject->SetRenderable(false);
            hiddenPlayerObjects.push_back(playerCharacter);
        }

        // Hide all child entities that have object components (character meshes, accessories, etc.)
        for (size_t i = 0; i < scene.hierarchy.GetCount(); i++) {
            wi::ecs::Entity entity = scene.hierarchy.GetEntity(i);
            wi::scene::HierarchyComponent *hierarchy = scene.hierarchy.GetComponent(entity);

            // Check if this entity is a descendant of the player (direct or indirect)
            wi::ecs::Entity parent = hierarchy->parentID;
            bool isPlayerDescendant = false;
            while (parent != wi::ecs::INVALID_ENTITY) {
                if (parent == playerCharacter) {
                    isPlayerDescendant = true;
                    break;
                }
                wi::scene::HierarchyComponent *parentHierarchy =
                    scene.hierarchy.GetComponent(parent);
                parent = parentHierarchy ? parentHierarchy->parentID : wi::ecs::INVALID_ENTITY;
            }

            if (isPlayerDescendant) {
                wi::scene::ObjectComponent *childObject = scene.objects.GetComponent(entity);
                if (childObject) {
                    childObject->SetRenderable(false);
                    hiddenPlayerObjects.push_back(entity);
                }
            }
        }

        char buf[128];
        sprintf_s(buf, "EnterCameraMode: Hidden %zu player objects\n", hiddenPlayerObjects.size());
        wi::backlog::post(buf);
    }

    // Hide aim dot and crosshair
    aimDotVisible = false;

    // Show camera panel
    if (cameraPanel) {
        cameraPanel->SetVisibility(Noesis::Visibility_Visible);
    }

    // Update photo count display
    UpdateCameraPhotoCount();

    // Update viewfinder layout (corner brackets need to be positioned based on window size)
    UpdateViewfinderLayout();

    wi::backlog::post("Entered camera mode\n");
}

// Exit camera mode
void NoesisRenderPath::ExitCameraMode() {
    if (!inCameraMode)
        return;

    inCameraMode = false;

    // Restore player character visibility by setting SetRenderable(true) on all hidden objects
    if (!hiddenPlayerObjects.empty()) {
        wi::scene::Scene &scene = wi::scene::GetScene();
        for (wi::ecs::Entity entity : hiddenPlayerObjects) {
            wi::scene::ObjectComponent *object = scene.objects.GetComponent(entity);
            if (object) {
                object->SetRenderable(true);
            }
        }
        char buf[128];
        sprintf_s(buf, "ExitCameraMode: Restored %zu player objects\n", hiddenPlayerObjects.size());
        wi::backlog::post(buf);
        hiddenPlayerObjects.clear();
    }

    // Switch back to third-person camera
    SetFirstPersonMode(false); // Disable first-person mode
    SetThirdPersonMode(true);  // Restore third-person camera following

    // Hide camera panel
    if (cameraPanel) {
        cameraPanel->SetVisibility(Noesis::Visibility_Collapsed);
    }

    wi::backlog::post("Exited camera mode\n");
}

// Take a photo (triggers shutter animation)
void NoesisRenderPath::TakePhoto() {
    if (!inCameraMode || shutterActive)
        return;

    wi::backlog::post("Taking photo...\n");

    // Request capture at end of frame (after rendering completes)
    // The actual capture happens in PostRender()
    captureRequestPending = true;

    // Trigger shutter animation
    shutterActive = true;
    shutterTime = 0.0f;
    photoCapturedThisShutter = false;
}

// Capture the current frame to memory (before shutter obscures it)
void NoesisRenderPath::CaptureFrameToMemory() {
    wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
    if (!device) {
        wi::backlog::post("CaptureFrameToMemory: No graphics device!",
                          wi::backlog::LogLevel::Error);
        return;
    }

    // Try GetLastPostprocessRT first (the actual 3D render), fall back to GetRenderResult
    const wi::graphics::Texture *renderResultPtr = GetLastPostprocessRT();
    if (!renderResultPtr || !renderResultPtr->IsValid()) {
        renderResultPtr = &GetRenderResult();
    }
    if (!renderResultPtr || !renderResultPtr->IsValid()) {
        wi::backlog::post("CaptureFrameToMemory: No valid render result",
                          wi::backlog::LogLevel::Error);
        return;
    }

    const wi::graphics::Texture &renderResult = *renderResultPtr;
    wi::graphics::TextureDesc texDesc = renderResult.GetDesc();

    // Step 1: Get raw GPU data
    wi::vector<uint8_t> rawData;
    if (!wi::helper::saveTextureToMemory(renderResult, rawData)) {
        wi::backlog::post("CaptureFrameToMemory: Failed to read texture",
                          wi::backlog::LogLevel::Error);
        return;
    }

    // Step 2: Convert to RGBA8 (4 bytes per pixel) with 4x downsample
    std::vector<uint8_t> rgba8Data;
    uint32_t downsampledWidth, downsampledHeight;
    if (!ConvertToRGBA8(rawData, texDesc, rgba8Data, downsampledWidth, downsampledHeight, 4)) {
        wi::backlog::post("CaptureFrameToMemory: Format conversion failed",
                          wi::backlog::LogLevel::Error);
        return;
    }

    // Step 3: Crop to center 1/3 (after downsampling)
    uint32_t cropWidth = downsampledWidth / 3;
    uint32_t cropHeight = downsampledHeight / 3;
    uint32_t cropX = downsampledWidth / 3;
    uint32_t cropY = downsampledHeight / 3;

    std::vector<uint8_t> croppedData(cropWidth * cropHeight * 4);
    for (uint32_t y = 0; y < cropHeight; ++y) {
        const uint8_t *srcRow = rgba8Data.data() + (cropY + y) * downsampledWidth * 4 + cropX * 4;
        uint8_t *dstRow = croppedData.data() + y * cropWidth * 4;
        std::memcpy(dstRow, srcRow, cropWidth * 4);
    }

    // Step 4: Apply post-processing (sepia, grain)
    ApplySepia(croppedData, cropWidth, cropHeight);
    AddFilmGrain(croppedData, cropWidth, cropHeight);

    // Step 5: Write PNG directly using stb_image_write
    CreateDirectoryA("SavedGameData", NULL);
    CreateDirectoryA("SavedGameData/Photos", NULL);
    photosTaken++;
    const std::string photo_filename =
        "SavedGameData/Photos/photo_" + std::to_string(photosTaken) + ".png";

    if (!stbi_write_png(photo_filename.c_str(), cropWidth, cropHeight, 4, croppedData.data(),
                        cropWidth * 4)) {
        wi::backlog::post("CaptureFrameToMemory: Failed to write PNG",
                          wi::backlog::LogLevel::Error);
        return;
    }

    UpdateCameraPhotoCount();
    AddPhotoCard(photo_filename);
}

// Convert raw GPU texture data to RGBA8 format with optional downsampling
bool NoesisRenderPath::ConvertToRGBA8(const wi::vector<uint8_t> &rawData,
                                      const wi::graphics::TextureDesc &desc,
                                      std::vector<uint8_t> &rgba8Data,
                                      uint32_t& outWidth,
                                      uint32_t& outHeight,
                                      uint32_t downsampleFactor) {
    using namespace wi::graphics;

    const uint32_t srcWidth = desc.width;
    const uint32_t srcHeight = desc.height;
    const uint32_t pixelCount = srcWidth * srcHeight;

    // Step 1: Convert to RGBA8 at full resolution
    std::vector<uint8_t> fullResRGBA8(pixelCount * 4);

    // Handle common formats
    if (desc.format == Format::R8G8B8A8_UNORM || desc.format == Format::R8G8B8A8_UNORM_SRGB) {
        // Already RGBA8, just copy
        std::memcpy(fullResRGBA8.data(), rawData.data(), pixelCount * 4);
    } else if (desc.format == Format::B8G8R8A8_UNORM ||
               desc.format == Format::B8G8R8A8_UNORM_SRGB) {
        // BGRA to RGBA
        const uint32_t *src = (const uint32_t *)rawData.data();
        uint32_t *dst = (uint32_t *)fullResRGBA8.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            uint32_t pixel = src[i];
            uint8_t b = (pixel >> 0) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t a = (pixel >> 24) & 0xFF;
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
    } else if (desc.format == Format::R16G16B16A16_FLOAT) {
        // Float16 to RGBA8
        const XMHALF4 *src = (const XMHALF4 *)rawData.data();
        uint32_t *dst = (uint32_t *)fullResRGBA8.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            float r = std::clamp(XMConvertHalfToFloat(src[i].x), 0.0f, 1.0f);
            float g = std::clamp(XMConvertHalfToFloat(src[i].y), 0.0f, 1.0f);
            float b = std::clamp(XMConvertHalfToFloat(src[i].z), 0.0f, 1.0f);
            float a = std::clamp(XMConvertHalfToFloat(src[i].w), 0.0f, 1.0f);
            dst[i] = ((uint32_t)(r * 255) << 0) | ((uint32_t)(g * 255) << 8) |
                     ((uint32_t)(b * 255) << 16) | ((uint32_t)(a * 255) << 24);
        }
    } else if (desc.format == Format::R10G10B10A2_UNORM) {
        // 10-bit to 8-bit
        const uint32_t *src = (const uint32_t *)rawData.data();
        uint32_t *dst = (uint32_t *)fullResRGBA8.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            uint32_t pixel = src[i];
            uint8_t r = (uint8_t)(((pixel >> 0) & 1023) * 255 / 1023);
            uint8_t g = (uint8_t)(((pixel >> 10) & 1023) * 255 / 1023);
            uint8_t b = (uint8_t)(((pixel >> 20) & 1023) * 255 / 1023);
            uint8_t a = (uint8_t)(((pixel >> 30) & 3) * 255 / 3);
            dst[i] = r | (g << 8) | (b << 16) | (a << 24);
        }
    } else if (desc.format == Format::R11G11B10_FLOAT) {
        // R11G11B10 float to RGBA8
        const XMFLOAT3PK *src = (const XMFLOAT3PK *)rawData.data();
        uint32_t *dst = (uint32_t *)fullResRGBA8.data();
        for (uint32_t i = 0; i < pixelCount; ++i) {
            XMFLOAT3PK pixel = src[i];
            XMVECTOR V = XMLoadFloat3PK(&pixel);
            XMFLOAT3 pixel3;
            XMStoreFloat3(&pixel3, V);
            float r = std::clamp(pixel3.x, 0.0f, 1.0f);
            float g = std::clamp(pixel3.y, 0.0f, 1.0f);
            float b = std::clamp(pixel3.z, 0.0f, 1.0f);
            dst[i] = ((uint32_t)(r * 255) << 0) | ((uint32_t)(g * 255) << 8) |
                     ((uint32_t)(b * 255) << 16) | (255 << 24);
        }
    } else {
        return false; // Unsupported format
    }

    // Step 2: Downsample if requested (using box filter)
    if (downsampleFactor > 1) {
        outWidth = srcWidth / downsampleFactor;
        outHeight = srcHeight / downsampleFactor;
        rgba8Data.resize(outWidth * outHeight * 4);

        for (uint32_t dstY = 0; dstY < outHeight; ++dstY) {
            for (uint32_t dstX = 0; dstX < outWidth; ++dstX) {
                uint32_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                uint32_t sampleCount = 0;

                // Sample a downsampleFactor x downsampleFactor block
                for (uint32_t dy = 0; dy < downsampleFactor; ++dy) {
                    for (uint32_t dx = 0; dx < downsampleFactor; ++dx) {
                        uint32_t srcX = dstX * downsampleFactor + dx;
                        uint32_t srcY = dstY * downsampleFactor + dy;
                        
                        if (srcX < srcWidth && srcY < srcHeight) {
                            uint32_t srcIdx = (srcY * srcWidth + srcX) * 4;
                            sumR += fullResRGBA8[srcIdx + 0];
                            sumG += fullResRGBA8[srcIdx + 1];
                            sumB += fullResRGBA8[srcIdx + 2];
                            sumA += fullResRGBA8[srcIdx + 3];
                            sampleCount++;
                        }
                    }
                }

                // Average and write output pixel
                if (sampleCount > 0) {
                    uint32_t dstIdx = (dstY * outWidth + dstX) * 4;
                    rgba8Data[dstIdx + 0] = (uint8_t)(sumR / sampleCount);
                    rgba8Data[dstIdx + 1] = (uint8_t)(sumG / sampleCount);
                    rgba8Data[dstIdx + 2] = (uint8_t)(sumB / sampleCount);
                    rgba8Data[dstIdx + 3] = (uint8_t)(sumA / sampleCount);
                }
            }
        }
    } else {
        // No downsampling, just return full res
        outWidth = srcWidth;
        outHeight = srcHeight;
        rgba8Data = std::move(fullResRGBA8);
    }

    return true;
}

// Save processed photo pixels to a PNG file, returns the filename
// Save processed pixels to a PNG file
bool NoesisRenderPath::SaveProcessedPhoto(const std::string &filename,
                                          const std::vector<uint8_t> &pixels, int width,
                                          int height) {
    // Create texture desc for the processed image
    wi::graphics::TextureDesc photoDesc;
    photoDesc.width = width;
    photoDesc.height = height;
    photoDesc.format = wi::graphics::Format::R8G8B8A8_UNORM;
    photoDesc.mip_levels = 1;
    photoDesc.array_size = 1;

    // Convert std::vector to wi::vector for compatibility
    wi::vector<uint8_t> wiPixels(pixels.begin(), pixels.end());

    // Save to file using Wicked Engine's helper (overwrites existing file)
    return wi::helper::saveTextureToFile(wiPixels, photoDesc, filename);
}

// Apply sepia filter to pixel data (RGBA format)
void NoesisRenderPath::ApplySepia(std::vector<uint8_t> &pixels, int width, int height) {
    for (int i = 0; i < width * height; i++) {
        int idx = i * 4; // RGBA

        // Get original colors (0-1 range)
        float r = pixels[idx + 0] / 255.0f;
        float g = pixels[idx + 1] / 255.0f;
        float b = pixels[idx + 2] / 255.0f;

        // Calculate luminance (preserving perceived brightness)
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

        // Create target sepia color based on luminance
        // For dark areas: darker brown (R=0.4, G=0.3, B=0.2)
        // For bright areas: very light warm brown (R=0.95, G=0.9, B=0.8)
        float sepiaTargetR = 0.4f + luminance * 0.55f;
        float sepiaTargetG = 0.3f + luminance * 0.60f;
        float sepiaTargetB = 0.2f + luminance * 0.60f;

        // Blend between original and sepia based on luminance
        float sepiaStrength = 0.7f + luminance * 0.3f;
        sepiaStrength = std::clamp(sepiaStrength, 0.7f, 1.0f);

        // Apply sepia blend
        float finalR = r + (sepiaTargetR - r) * sepiaStrength;
        float finalG = g + (sepiaTargetG - g) * sepiaStrength;
        float finalB = b + (sepiaTargetB - b) * sepiaStrength;

        // Clamp and store
        pixels[idx + 0] = (uint8_t)std::clamp(finalR * 255.0f, 0.0f, 255.0f);
        pixels[idx + 1] = (uint8_t)std::clamp(finalG * 255.0f, 0.0f, 255.0f);
        pixels[idx + 2] = (uint8_t)std::clamp(finalB * 255.0f, 0.0f, 255.0f);
        // Alpha unchanged
    }
}

// Add film grain effect to pixel data
void NoesisRenderPath::AddFilmGrain(std::vector<uint8_t> &pixels, int width, int height) {
    float grainIntensity = 8.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;

            // Use simple pseudo-random grain based on position
            float noiseX = x / 48.0f;
            float noiseY = y / 48.0f;

            // Simple noise approximation (sine-based for speed)
            float grain = sinf(noiseX * 127.1f + noiseY * 311.7f) * 0.3f;
            grain += sinf(noiseX * 269.5f + noiseY * 183.3f) * 0.2f;
            grain += sinf(noiseX * 419.2f + noiseY * 371.9f) * 0.1f;

            // Random component (using pixel position as seed)
            grain += ((x * 7919 + y * 7927) % 1000) / 5000.0f - 0.1f;

            // Apply grain with luminance-aware intensity
            float luminance =
                (pixels[idx] * 0.299f + pixels[idx + 1] * 0.587f + pixels[idx + 2] * 0.114f) /
                255.0f;
            float adaptiveIntensity = grainIntensity * (0.5f + luminance * 0.5f);

            int grainValue = (int)(grain * adaptiveIntensity);

            // Apply to RGB channels
            pixels[idx + 0] = (uint8_t)std::clamp((int)pixels[idx + 0] + grainValue, 32, 223);
            pixels[idx + 1] = (uint8_t)std::clamp((int)pixels[idx + 1] + grainValue, 32, 223);
            pixels[idx + 2] = (uint8_t)std::clamp((int)pixels[idx + 2] + grainValue, 32, 223);
        }
    }
}

// Add a photo card to the caseboard with the image from file
void NoesisRenderPath::AddPhotoCard(const std::string &photoFilename) {
    if (!caseboardContent) {
        wi::backlog::post("AddPhotoCard: caseboardContent is null!\n");
        return;
    }

    wi::backlog::post("AddPhotoCard: Creating photo card...\n");

    // Calculate position in board space (offset for each new photo)
    float boardX = 500.0f + (photoCards.size() % 5) * 200.0f;
    float boardY = -400.0f + (photoCards.size() / 5) * 280.0f;

    // Create the photo card container
    Noesis::Ptr<Noesis::Grid> cardContainer = *new Noesis::Grid();
    cardContainer->SetWidth(180.0f);
    cardContainer->SetHeight(250.0f);

    // Create a border with vintage photo style (cream/brown frame)
    Noesis::Ptr<Noesis::Border> frame = *new Noesis::Border();
    frame->SetBackground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(0xF5, 0xF0, 0xE6))); // Cream/paper color
    frame->SetBorderBrush(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x8B, 0x73, 0x55))); // Brown
    frame->SetBorderThickness(Noesis::Thickness(4.0f));
    frame->SetPadding(Noesis::Thickness(10.0f, 10.0f, 10.0f, 35.0f)); // Extra bottom for label

    // Inner content grid
    Noesis::Ptr<Noesis::Grid> innerGrid = *new Noesis::Grid();

    // Load the photo image
    Noesis::Ptr<Noesis::Image> photoImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> bitmapSource = *new Noesis::BitmapImage();

    // Use the full path as-is (SavedGameData is relative to executable)
    bitmapSource->SetUriSource(Noesis::Uri(photoFilename.c_str()));
    photoImage->SetSource(bitmapSource);
    photoImage->SetStretch(Noesis::Stretch_Uniform);
    photoImage->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    innerGrid->GetChildren()->Add(photoImage);

    frame->SetChild(innerGrid);
    cardContainer->GetChildren()->Add(frame);

    // Add label at the bottom
    Noesis::Ptr<Noesis::TextBlock> label = *new Noesis::TextBlock();
    std::string labelText = "Photo #" + std::to_string(photosTaken);
    label->SetText(labelText.c_str());
    label->SetFontSize(12.0f);
    label->SetFontWeight(Noesis::FontWeight_Normal);
    label->SetForeground(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x4A, 0x3C, 0x28)));
    label->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    label->SetVerticalAlignment(Noesis::VerticalAlignment_Bottom);
    label->SetMargin(Noesis::Thickness(0.0f, 0.0f, 0.0f, 8.0f));
    label->SetEffect(nullptr);
    cardContainer->GetChildren()->Add(label);

    // Add drop shadow effect
    Noesis::Ptr<Noesis::DropShadowEffect> shadow = *new Noesis::DropShadowEffect();
    shadow->SetColor(Noesis::Color(0, 0, 0));
    shadow->SetBlurRadius(12.0f);
    shadow->SetShadowDepth(5.0f);
    shadow->SetOpacity(0.6f);
    cardContainer->SetEffect(shadow);

    // Position the card on the canvas
    Noesis::Canvas::SetLeft(cardContainer, boardX - 90.0f);
    Noesis::Canvas::SetTop(cardContainer, boardY - 125.0f);

    // Add to the caseboard content
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(cardContainer);
        char buf[128];
        sprintf_s(buf, "AddPhotoCard: Added to caseboard, now has %d children\n",
                  children->Count());
        wi::backlog::post(buf);
    } else {
        wi::backlog::post("AddPhotoCard: Could not get children collection!\n");
    }

    // Store reference
    PhotoCard card;
    card.container = cardContainer;
    card.photoImage = photoImage;
    card.label = label;
    card.boardX = boardX;
    card.boardY = boardY;
    card.labelText = labelText;
    photoCards.push_back(card);
}

// Simulate shutter animation (call from Update)
void NoesisRenderPath::SimulateShutter(float deltaSeconds) {
    if (!shutterActive)
        return;

    shutterTime += deltaSeconds;

    float t = shutterTime / shutterDuration;

    if (t >= 1.0f) {
        // End of animation
        shutterActive = false;
        shutterAlpha = 0.0f;
        photoCapturedThisShutter = false;
    } else {
        // Animate in & out (0 → 1 → 0 over the animation time)
        float phase = (t <= 0.5f) ? (t * 2.0f)         // closing
                                  : (2.0f - t * 2.0f); // opening

        // Process photo at the midpoint (when fully closed)
        // Only do this once per shutter cycle
        if (t >= 0.5f && !photoCapturedThisShutter) {
            photoCapturedThisShutter = true;
        }

        shutterAlpha = std::clamp(phase, 0.0f, 1.0f);
    }

    // Update shutter bar heights
    UpdateShutterBars();
}

// Update shutter bar heights based on shutterAlpha
void NoesisRenderPath::UpdateShutterBars() {
    if (!windowHandle)
        return;

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    float viewportHeight = (float)(clientRect.bottom - clientRect.top);

    // Shutter bars close from top and bottom
    float barHeight = viewportHeight * 0.5f * shutterAlpha;

    if (shutterBarTop) {
        shutterBarTop->SetHeight(barHeight);
    }
    if (shutterBarBottom) {
        shutterBarBottom->SetHeight(barHeight);
    }
}

// Update the photo count display
void NoesisRenderPath::UpdateCameraPhotoCount() {
    if (!cameraPhotoCount)
        return;

    char buf[64];
    sprintf_s(buf, "PHOTOS: %d", photosTaken);
    cameraPhotoCount->SetText(buf);
}

// Update viewfinder layout based on window size (now a no-op since viewfinder is just an image)
void NoesisRenderPath::UpdateViewfinderLayout() {
    // Viewfinder is now a single image that stretches to fill the panel
}

// Handle mouse click in camera mode
void NoesisRenderPath::CameraClick(int x, int y) {
    if (!inCameraMode)
        return;

    // Any click in camera mode takes a photo
    TakePhoto();
}

bool NoesisRenderPath::TryHandleShortcut(Noesis::Key key) {
    // Ignore unknown keys
    if (key == Noesis::Key_None)
        return false;

    if (inMainMenuMode) {
        // Forward to Noesis (for TextBox input, etc.)
        return GetNoesisView()->KeyDown(key);
    }

    // Handle camera mode shortcuts
    if (inCameraMode) {
        switch (key) {
        case Noesis::Key_Escape:
        case Noesis::Key_Tab:
            ExitCameraMode();
            return true; // Consumed
        case Noesis::Key_Space:
            TakePhoto();
            return true; // Consumed
        default:
            return true; // Consume all keys in camera mode (no Noesis UI interaction)
        }
    }

    if (inCaseboardMode) {
        // If editing, let Noesis handle all keys
        if (editingNoteCardIndex >= 0) {
            // Exception: Escape always exits/finalizes
            if (key == Noesis::Key_Escape) {
                FinalizeNoteCardEdit();
                return true; // Consumed
            }
            // Forward to Noesis (for TextBox input, etc.)
            return GetNoesisView()->KeyDown(key);
        }

        // Not editing - handle shortcuts
        switch (key) {
        case Noesis::Key_Escape:
        case Noesis::Key_C:
            ExitCaseboardMode();
            return true; // Consumed
        case Noesis::Key_N:
            AddNoteCard();
            return true; // Consumed
        default:
            // Forward to Noesis (for TextBox input, etc.)
            return GetNoesisView()->KeyDown(key);
        }
    }

    // Handle dialogue mode shortcuts
    if (inDialogueMode) {
        if (key == Noesis::Key_Escape) {
            ExitDialogueMode();
            return true; // Consumed
        }
        // Forward to Noesis (for TextBox input, etc.)
        return GetNoesisView()->KeyDown(key);
    }

    // If we get here, then no GUI mode is active and this is a top-level shortcut -- Noesis is
    // not involved.
    switch (key) {
    case Noesis::Key_T:
        if (aimedNPCEntity != wi::ecs::INVALID_ENTITY) {
            EnterDialogueMode(aimedNPCEntity);
        }
        return true; // Consumed
    case Noesis::Key_C:
        EnterCaseboardMode();
        return true; // Consumed
    case Noesis::Key_Tab:
        EnterCameraMode();
        return true; // Consumed
    default:
        return false; // Not handled
    }
}

// Config file management
std::string NoesisRenderPath::GetConfigFilePath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Extract directory
    std::wstring path = exePath;
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        path = path.substr(0, lastSlash + 1);
    }
    path += L"config.ini";

    // Convert wide string to UTF-8
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    if (size_needed > 0) {
        std::vector<char> buffer(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, buffer.data(), size_needed, NULL, NULL);
        return std::string(buffer.data());
    }
    return "config.ini"; // Fallback
}

void NoesisRenderPath::SaveConfig() {
    std::string configPath = GetConfigFilePath();
    std::ofstream configFile(configPath);
    if (configFile.is_open()) {
        configFile << "project_path = " << projectPath << "\n";
        configFile << "theme_music = " << themeMusic << "\n";
        configFile << "level = " << levelPath << "\n";
        configFile << "player_model = " << playerModel << "\n";
        configFile << "npc_model = " << npcModel << "\n";
        configFile << "anim_lib = " << animLib << "\n";
        configFile << "expression_path = " << expressionPath << "\n";
        configFile.close();

        char buffer[512];
        sprintf_s(buffer, "Saved config: project_path = %s\n", projectPath.c_str());
        wi::backlog::post(buffer);
    }
}

void NoesisRenderPath::LoadConfig() {
    std::string configPath = GetConfigFilePath();
    std::ifstream configFile(configPath);
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            // Parse INI format: key = value
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                // Trim whitespace from key and value
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                // Remove quotes from value if present
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }

                if (key == "project_path") {
                    projectPath = value;
                } else if (key == "theme_music") {
                    themeMusic = value;
                } else if (key == "level") {
                    levelPath = value;
                } else if (key == "player_model") {
                    playerModel = value;
                } else if (key == "npc_model") {
                    npcModel = value;
                } else if (key == "anim_lib") {
                    animLib = value;
                } else if (key == "expression_path") {
                    expressionPath = value;
                }
            }
        }
        configFile.close();

        char buffer[512];
        sprintf_s(buffer, "Loaded config:\n  project_path = %s\n  theme_music = %s\n  level = %s\n",
                  projectPath.c_str(), themeMusic.c_str(), levelPath.c_str());
        wi::backlog::post(buffer);
    }

    // Update control states based on project path
    UpdateControlStates();
}

void NoesisRenderPath::InitializeAnimationSystem() {
    if (projectPath.empty()) {
        wi::backlog::post("Cannot initialize animation system: project_path not set\n");
        return;
    }

    wi::scene::Scene &scene = wi::scene::GetScene();

    // Load animation library if specified
    if (!animLib.empty()) {
        std::string fullAnimLibPath = projectPath;
        if (!fullAnimLibPath.empty() && fullAnimLibPath.back() != '/' &&
            fullAnimLibPath.back() != '\\') {
            fullAnimLibPath += "\\";
        }
        fullAnimLibPath += animLib;

        // Normalize path separators
        for (char &c : fullAnimLibPath) {
            if (c == '/')
                c = '\\';
        }

        char buffer[512];
        sprintf_s(buffer, "Loading animation library: %s\n", fullAnimLibPath.c_str());
        wi::backlog::post(buffer);

        if (wi::helper::FileExists(fullAnimLibPath)) {
            // Load animation library scene
            wi::scene::Scene animScene;
            wi::Archive archive(fullAnimLibPath);
            archive.SetSourceDirectory(wi::helper::InferProjectPath(fullAnimLibPath));
            if (archive.IsOpen()) {
                animScene.Serialize(archive);

                // Store animation entities for retargeting
                animationLibrary.clear();
                for (size_t a = 0; a < animScene.animations.GetCount(); ++a) {
                    wi::ecs::Entity anim_entity = animScene.animations.GetEntity(a);
                    animationLibrary.push_back(anim_entity);

                    const wi::scene::NameComponent *anim_name =
                        animScene.names.GetComponent(anim_entity);
                    sprintf_s(buffer, "Found animation '%s' (entity: %llu)\n",
                              anim_name ? anim_name->name.c_str() : "unnamed", anim_entity);
                    wi::backlog::post(buffer);
                }

                // Merge animation library into main scene
                scene.Merge(animScene);

                sprintf_s(buffer, "Animation library loaded with %d animations\n",
                          (int)animationLibrary.size());
                wi::backlog::post(buffer);
            } else {
                sprintf_s(buffer, "ERROR: Failed to open animation library archive: %s\n",
                          fullAnimLibPath.c_str());
                wi::backlog::post(buffer);
            }
        } else {
            sprintf_s(buffer, "ERROR: Animation library not found: %s\n", fullAnimLibPath.c_str());
            wi::backlog::post(buffer);
        }
    } else {
        wi::backlog::post("No animation library (anim_lib) configured in config.ini\n");
    }

    // Load expressions (required by action system, same as walkabout mode)
    if (!expressionPath.empty()) {
        char buffer[512];
        sprintf_s(buffer, "Loading expressions from: %s (project: %s)\n", expressionPath.c_str(),
                  projectPath.c_str());
        wi::backlog::post(buffer);

        // Use the same method as walkabout mode
        scene.LoadExpressions(expressionPath, projectPath);

        wi::backlog::post("Expressions loaded successfully\n");
    } else {
        wi::backlog::post("No expression path (expression_path) configured in config.ini\n");
    }
}

void NoesisRenderPath::UpdateControlStates() {
    // Check if project path is set from config file
    bool hasProjectPath = !projectPath.empty();

    // Enable/disable controls based on project path
    if (seedTextBox) {
        seedTextBox->SetIsEnabled(hasProjectPath);
    }
    if (playGameButton) {
        playGameButton->SetIsEnabled(hasProjectPath);
    }
    if (fullscreenButton) {
        fullscreenButton->SetIsEnabled(hasProjectPath);
    }

    // Load and play music if project path is set and music isn't already playing
    if (hasProjectPath && !menuMusicInstance.IsValid()) {
        LoadAndPlayMenuMusic();
    }
    // Stop music if project path is cleared
    else if (!hasProjectPath && menuMusicInstance.IsValid()) {
        wi::audio::Stop(&menuMusicInstance);
        menuMusicInstance = {};
        menuMusic = {};
    }
}

void NoesisRenderPath::LoadAndPlayMenuMusic() {
    // Use theme_music from config, or fall back to default
    if (themeMusic.empty()) {
        wi::backlog::post("No theme_music configured in config.ini\n");
        return;
    }

    // Construct full music file path
    // WickedEngine supports WAV, OGG Vorbis, and MP3 formats
    std::string musicPath = projectPath;
    if (!musicPath.empty() && musicPath.back() != '/' && musicPath.back() != '\\') {
        musicPath += "\\";
    }
    musicPath += themeMusic;

    // Normalize path separators
    for (char &c : musicPath) {
        if (c == '/')
            c = '\\';
    }

    // Check if file exists
    if (!wi::helper::FileExists(musicPath)) {
        // Try alternate formats
        std::string oggPath = wi::helper::ReplaceExtension(musicPath, "ogg");
        std::string wavPath = wi::helper::ReplaceExtension(musicPath, "wav");

        if (wi::helper::FileExists(oggPath)) {
            musicPath = oggPath;
        } else if (wi::helper::FileExists(wavPath)) {
            musicPath = wavPath;
        } else {
            char buffer[512];
            sprintf_s(buffer, "Music file not found: %s (tried .mp3, .ogg, .wav)\n",
                      musicPath.c_str());
            wi::backlog::post(buffer);
            return;
        }
    }

    // Create sound from file
    if (wi::audio::CreateSound(musicPath, &menuMusic)) {
        // Configure music instance
        menuMusicInstance.type = wi::audio::SUBMIX_TYPE_MUSIC;
        menuMusicInstance.SetLooped(true);

        // Create sound instance and play
        if (wi::audio::CreateSoundInstance(&menuMusic, &menuMusicInstance)) {
            wi::audio::Play(&menuMusicInstance);

            char buffer[512];
            sprintf_s(buffer, "Started playing menu music: %s\n", musicPath.c_str());
            wi::backlog::post(buffer);
        } else {
            wi::backlog::post("Failed to create music instance\n");
        }
    } else {
        char buffer[512];
        sprintf_s(buffer, "Failed to load music: %s\n", musicPath.c_str());
        wi::backlog::post(buffer);
    }
}

// Helper to find animation entity by name substring (like walkabout)
wi::ecs::Entity NoesisRenderPath::FindAnimationByName(const wi::scene::Scene &scene,
                                                      const char *anim_name_substr) {
    for (wi::ecs::Entity lib_anim : animationLibrary) {
        const wi::scene::NameComponent *anim_name = scene.names.GetComponent(lib_anim);
        if (anim_name) {
            std::string name_lower = anim_name->name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (name_lower.find(anim_name_substr) != std::string::npos) {
                return lib_anim;
            }
        }
    }
    return wi::ecs::INVALID_ENTITY;
}

void NoesisRenderPath::SpawnCharactersFromMetadata(wi::scene::Scene &scene) {
    char buffer[512];

    // Find all entities with metadata components
    for (size_t i = 0; i < scene.metadatas.GetCount(); i++) {
        wi::ecs::Entity entity = scene.metadatas.GetEntity(i);
        const wi::scene::MetadataComponent &metadata = scene.metadatas[i];

        // Get the transform of the spawn point
        wi::scene::TransformComponent *spawnTransform = scene.transforms.GetComponent(entity);
        if (!spawnTransform) {
            continue; // Need a transform to know where to spawn
        }

        XMFLOAT3 spawnPos = spawnTransform->GetPosition();
        XMFLOAT3 spawnForward = spawnTransform->GetForward();

        // Check if this is a Player spawn point
        if (metadata.preset == wi::scene::MetadataComponent::Preset::Player) {
            if (playerModel.empty()) {
                sprintf_s(
                    buffer,
                    "Player spawn found at (%.2f, %.2f, %.2f) but no player_model configured\n",
                    spawnPos.x, spawnPos.y, spawnPos.z);
                wi::backlog::post(buffer);
                continue;
            }

            sprintf_s(buffer, "Spawning player at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                      spawnPos.z);
            wi::backlog::post(buffer);

            SpawnCharacter(scene, playerModel, spawnPos, spawnForward, true);
        }
        // Check if this is an NPC spawn point
        else if (metadata.preset == wi::scene::MetadataComponent::Preset::NPC) {
            if (npcModel.empty()) {
                sprintf_s(buffer,
                          "NPC spawn found at (%.2f, %.2f, %.2f) but no npc_model configured\n",
                          spawnPos.x, spawnPos.y, spawnPos.z);
                wi::backlog::post(buffer);
                continue;
            }

            sprintf_s(buffer, "Spawning NPC at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y,
                      spawnPos.z);
            wi::backlog::post(buffer);

            SpawnCharacter(scene, npcModel, spawnPos, spawnForward, false);
        }
    }
}

wi::ecs::Entity NoesisRenderPath::SpawnCharacter(wi::scene::Scene &scene,
                                                 const std::string &modelPath,
                                                 const XMFLOAT3 &position, const XMFLOAT3 &forward,
                                                 bool isPlayer) {
    // Construct full model path
    std::string fullModelPath = projectPath;
    if (!fullModelPath.empty() && fullModelPath.back() != '/' && fullModelPath.back() != '\\') {
        fullModelPath += "\\";
    }
    fullModelPath += modelPath;

    // Normalize path separators
    for (char &c : fullModelPath) {
        if (c == '/')
            c = '\\';
    }

    char buffer[512];
    sprintf_s(buffer, "Loading character model: %s\n", fullModelPath.c_str());
    wi::backlog::post(buffer);

    // Check if file exists
    if (!wi::helper::FileExists(fullModelPath)) {
        sprintf_s(buffer, "ERROR: Character model not found: %s\n", fullModelPath.c_str());
        wi::backlog::post(buffer);
        return wi::ecs::INVALID_ENTITY;
    }

    // Create spawn matrix
    XMMATRIX spawnMatrix = XMMatrixTranslation(position.x, position.y, position.z);

    // Load model into temporary scene
    wi::scene::Scene tempScene;
    wi::ecs::Entity modelRoot = wi::scene::LoadModel(tempScene, fullModelPath, spawnMatrix, true);

    if (modelRoot == wi::ecs::INVALID_ENTITY) {
        sprintf_s(buffer, "ERROR: Failed to load character model: %s\n", fullModelPath.c_str());
        wi::backlog::post(buffer);
        return wi::ecs::INVALID_ENTITY;
    }

    // Find humanoid component in loaded model
    wi::ecs::Entity humanoidEntity = wi::ecs::INVALID_ENTITY;
    for (size_t h = 0; h < tempScene.cc_humanoids.GetCount(); ++h) {
        wi::ecs::Entity hEntity = tempScene.cc_humanoids.GetEntity(h);
        if (hEntity == modelRoot || tempScene.Entity_IsDescendant(hEntity, modelRoot)) {
            humanoidEntity = hEntity;
            break;
        }
    }

    // Merge into main scene
    scene.Merge(tempScene);
    scene.ResetPose(modelRoot);

    // Create character entity (use model root)
    wi::ecs::Entity characterEntity = modelRoot;

    // Create CharacterComponent
    wi::scene::CharacterComponent &character = scene.characters.Create(characterEntity);
    character.SetPosition(position);
    character.SetFacing(forward);
    character.SetFootPlacementEnabled(true);
    character.width = 0.3f;   // 30cm radius capsule
    character.height = 1.86f; // 186cm tall
    character.turning_speed = 9.0f;
    character.gravity = -30.0f;
    character.fixed_update_fps = 120.0f;
    character.ground_friction = 0.92f;
    character.slope_threshold = 0.2f;

    // Create MindComponent for action system (like walkabout mode)
    wi::scene::MindComponent &mind = scene.minds.Create(characterEntity);

    // Create transform component if not already present
    wi::scene::TransformComponent *transform = scene.transforms.GetComponent(characterEntity);
    if (transform == nullptr) {
        transform = &scene.transforms.Create(characterEntity);
        transform->ClearTransform();
        transform->Translate(position);
        transform->UpdateTransform();
    }

    // Set layer for collision filtering
    wi::scene::LayerComponent *layer = scene.layers.GetComponent(characterEntity);
    if (layer == nullptr) {
        layer = &scene.layers.Create(characterEntity);
    }

    if (isPlayer) {
        // Player-specific setup
        mind.type = wi::scene::MindComponent::Type::Player;
        layer->layerMask = 1 << 0; // Player layer

        // Store player character reference for camera following
        playerCharacter = characterEntity;
    } else {
        // NPC-specific setup
        mind.type = wi::scene::MindComponent::Type::NPC;
        layer->layerMask = 1 << 1; // NPC layer

        // Assign script callback (50% patrol, 50% guard like walkabout)
        bool is_patrol = (rand() % 2) == 0;
        if (is_patrol) {
            mind.scriptCallback = "npc_patrol_update";
        } else {
            mind.scriptCallback = "npc_guard_update";
        }

        // Track NPC entity for Lua updates
        npcEntities.push_back(characterEntity);
    }

    // === RETARGET ANIMATIONS (like walkabout) ===
    if (modelRoot != wi::ecs::INVALID_ENTITY && !animationLibrary.empty()) {
        sprintf_s(buffer, "Retargeting animations for character (Entity: %llu)...\n",
                  characterEntity);
        wi::backlog::post(buffer);

        // Find required animations from library
        wi::ecs::Entity idle_anim = FindAnimationByName(scene, "idle");
        wi::ecs::Entity walk_anim = FindAnimationByName(scene, "walk");
        wi::ecs::Entity sit_anim = FindAnimationByName(scene, "sit");

        // Retarget and assign to character's action_anim_entities
        if (idle_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Idle] =
                wi::scene::animation_system::retarget(scene, idle_anim, modelRoot);
        }
        if (walk_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Walk] =
                wi::scene::animation_system::retarget(scene, walk_anim, modelRoot);
            // WalkTo uses same animation as Walk
            character.action_anim_entities[(int)wi::scene::ActionVerb::WalkTo] =
                character.action_anim_entities[(int)wi::scene::ActionVerb::Walk];
        }
        if (sit_anim != wi::ecs::INVALID_ENTITY) {
            character.action_anim_entities[(int)wi::scene::ActionVerb::Sit] =
                wi::scene::animation_system::retarget(scene, sit_anim, modelRoot);
            // Stand is sit played backwards
            character.action_anim_entities[(int)wi::scene::ActionVerb::Stand] =
                character.action_anim_entities[(int)wi::scene::ActionVerb::Sit];
        }

        // Play idle animation by default
        character.SetAction(scene, wi::scene::character_system::make_idle(scene, character));

        wi::backlog::post("Animations retargeted successfully\n");
    }

    // === BIND EXPRESSIONS (like walkabout) ===
    if (modelRoot != wi::ecs::INVALID_ENTITY && !scene.unbound_expressions.empty()) {
        sprintf_s(buffer, "Binding expressions for character (Entity: %llu)...\n", characterEntity);
        wi::backlog::post(buffer);

        int mapped_count = 0;
        for (size_t expr_index = 0; expr_index < scene.unbound_expressions.size(); expr_index++) {
            const auto &expr = scene.unbound_expressions[expr_index];
            auto morph_entities = scene.find_entities_with_morphs(modelRoot, expr);
            for (auto target_entity : morph_entities) {
                auto expr_comp = scene.demand_expression_component(target_entity);
                if (expr_comp->bind_expression(scene, expr_index, target_entity)) {
                    mapped_count++;
                    sprintf_s(buffer, "Mapped expression '%s' to entity %llu\n", expr.name.c_str(),
                              target_entity);
                    wi::backlog::post(buffer);
                }
            }
        }
        sprintf_s(buffer, "Bound %d of %d expressions\n", mapped_count,
                  (int)scene.unbound_expressions.size());
        wi::backlog::post(buffer);
    }

    sprintf_s(buffer, "%s character spawned successfully (Entity: %llu)\n",
              isPlayer ? "Player" : "NPC", characterEntity);
    wi::backlog::post(buffer);

    return characterEntity;
}

void NoesisRenderPath::LoadNPCScripts() {
    // Ensure project path ends with separator
    std::string basePath = projectPath;
    if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\') {
        basePath += "/";
    }

    // Try multiple locations for the patrol script (like walkabout)
    if (!patrolScriptLoaded) {
        std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/patrol.lua",
                                                 basePath + "SharedContent/scripts/npc/patrol.lua",
                                                 basePath + "scripts/npc/patrol.lua"};

        for (const auto &patrol_script_path : script_paths) {
            if (wi::helper::FileExists(patrol_script_path)) {
                wi::lua::RunFile(patrol_script_path);
                patrolScriptLoaded = true;
                char buffer[512];
                sprintf_s(buffer, "Loaded NPC patrol script from: %s\n",
                          patrol_script_path.c_str());
                wi::backlog::post(buffer);
                break;
            }
        }

        if (!patrolScriptLoaded) {
            wi::backlog::post("WARNING: NPC patrol script not found in any location\n");
        }
    }

    // Try multiple locations for the guard script (like walkabout)
    if (!guardScriptLoaded) {
        std::vector<std::string> script_paths = {basePath + "Content/scripts/npc/guard.lua",
                                                 basePath + "SharedContent/scripts/npc/guard.lua",
                                                 basePath + "scripts/npc/guard.lua"};

        for (const auto &guard_script_path : script_paths) {
            if (wi::helper::FileExists(guard_script_path)) {
                wi::lua::RunFile(guard_script_path);
                guardScriptLoaded = true;
                char buffer[512];
                sprintf_s(buffer, "Loaded NPC guard script from: %s\n", guard_script_path.c_str());
                wi::backlog::post(buffer);
                break;
            }
        }

        if (!guardScriptLoaded) {
            wi::backlog::post("WARNING: NPC guard script not found in any location\n");
        }
    }
}

void NoesisRenderPath::CleanupNPCScripts() {
    // Clean up Lua NPC script state (like walkabout)
    if (patrolScriptLoaded) {
        wi::lua::RunText("npc_patrol_clear_all()");
        patrolScriptLoaded = false;
    }
    if (guardScriptLoaded) {
        wi::lua::RunText("npc_guard_clear_all()");
        guardScriptLoaded = false;
    }
    npcEntities.clear();
}

void NoesisRenderPath::LoadGameScene() {
    if (levelPath.empty()) {
        wi::backlog::post("ERROR: No level path configured in config.ini\n");
        return;
    }

    // Construct full scene file path
    std::string scenePath = projectPath;
    if (!scenePath.empty() && scenePath.back() != '/' && scenePath.back() != '\\') {
        scenePath += "\\";
    }
    scenePath += levelPath;

    // Normalize path separators
    for (char &c : scenePath) {
        if (c == '/')
            c = '\\';
    }

    char buffer[512];
    sprintf_s(buffer, "Loading scene: %s\n", scenePath.c_str());
    wi::backlog::post(buffer);

    // Check if file exists
    if (!wi::helper::FileExists(scenePath)) {
        sprintf_s(buffer, "ERROR: Scene file not found: %s\n", scenePath.c_str());
        wi::backlog::post(buffer);
        return;
    }

    // Get the current scene
    wi::scene::Scene &scene = wi::scene::GetScene();

    // Clear existing scene
    scene.Clear();

    // Load the new scene
    wi::Archive archive(scenePath);
    archive.SetSourceDirectory(wi::helper::InferProjectPath(scenePath));
    if (archive.IsOpen()) {
        scene.Serialize(archive);

        wi::backlog::post("Scene loaded successfully\n");

        // Load animation library and expressions AFTER loading scene
        // (scene.Clear() would have wiped them if loaded earlier)
        InitializeAnimationSystem();

        // Update scene to compute bounds
        scene.Update(0.0f);

        // Spawn player and NPCs based on metadata components
        SpawnCharactersFromMetadata(scene);

        // Load NPC Lua scripts if NPCs were spawned (like walkabout)
        if (!npcEntities.empty()) {
            LoadNPCScripts();
        }

        // Initialize camera angles if player was spawned
        if (playerCharacter != wi::ecs::INVALID_ENTITY) {
            wi::scene::CharacterComponent *playerChar =
                scene.characters.GetComponent(playerCharacter);
            if (playerChar) {
                XMFLOAT3 playerForward = playerChar->GetFacing();
                cameraHorizontal = atan2f(playerForward.x, playerForward.z);
                cameraVertical = 0.3f;
                cameraDistance = 2.5f;

                wi::backlog::post("Camera initialized to follow player\n");
            }
        } else {
            wi::backlog::post("WARNING: No player character spawned, camera will not follow\n");
        }

    } else {
        sprintf_s(buffer, "ERROR: Failed to open scene archive: %s\n", scenePath.c_str());
        wi::backlog::post(buffer);
    }
}

void NoesisRenderPath::SetThirdPersonMode(bool enabled) {
    if (!windowHandle)
        return;

    isFirstPersonMode = enabled;

    if (enabled) {
        // Hide and capture the cursor (like editor walkabout mode)
        ShowCursor(FALSE);

        // Get window center for mouse reset
        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        POINT center = {(clientRect.right - clientRect.left) / 2,
                        (clientRect.bottom - clientRect.top) / 2};
        ClientToScreen(windowHandle, &center);
        SetCursorPos(center.x, center.y);

        // Clip cursor to window
        RECT windowRect;
        GetWindowRect(windowHandle, &windowRect);
        ClipCursor(&windowRect);

        mouseInitialized = false;

        char buffer[256];
        sprintf_s(buffer,
                  "Walkabout control mode enabled. Mouse captured for third-person camera.\n");
        wi::backlog::post(buffer);
    } else {
        // Show cursor and release capture
        ShowCursor(TRUE);
        ClipCursor(nullptr);

        mouseInitialized = false;

        wi::backlog::post("Walkabout control mode disabled. Mouse released.\n");
    }
}

// Enable/disable first-person camera mode (for camera/photography mode)
void NoesisRenderPath::SetFirstPersonMode(bool enabled) {
    if (!windowHandle)
        return;

    if (enabled) {
        // Hide and capture the cursor (same as third-person mode)
        ShowCursor(FALSE);

        // Get window center for mouse reset
        RECT clientRect;
        GetClientRect(windowHandle, &clientRect);
        POINT center = {(clientRect.right - clientRect.left) / 2,
                        (clientRect.bottom - clientRect.top) / 2};
        ClientToScreen(windowHandle, &center);
        SetCursorPos(center.x, center.y);

        // Clip cursor to window
        RECT windowRect;
        GetWindowRect(windowHandle, &windowRect);
        ClipCursor(&windowRect);

        mouseInitialized = false;

        wi::backlog::post("First-person camera mode enabled (for photography).\n");
    } else {
        // Show cursor and release capture
        ShowCursor(TRUE);
        ClipCursor(nullptr);

        mouseInitialized = false;

        wi::backlog::post("First-person camera mode disabled.\n");
    }
}

// Toggle fullscreen mode
void NoesisRenderPath::ToggleFullscreen() {
    if (!windowHandle)
        return;

    if (!isFullscreen) {
        // Save current window state
        windowedStyle = GetWindowLong(windowHandle, GWL_STYLE);
        windowedExStyle = GetWindowLong(windowHandle, GWL_EXSTYLE);
        GetWindowRect(windowHandle, &windowedRect);

        // Get monitor info
        MONITORINFO mi = {sizeof(mi)};
        if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            // Remove window decorations and set fullscreen style
            SetWindowLong(windowHandle, GWL_STYLE, windowedStyle & ~(WS_CAPTION | WS_THICKFRAME));
            SetWindowLong(windowHandle, GWL_EXSTYLE,
                          windowedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                              WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

            // Set window to cover entire monitor
            SetWindowPos(windowHandle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            isFullscreen = true;

            // Update button text
            if (fullscreenButton) {
                fullscreenButton->SetContent("WINDOWED");
            }
        }
    } else {
        // Restore windowed mode
        SetWindowLong(windowHandle, GWL_STYLE, windowedStyle);
        SetWindowLong(windowHandle, GWL_EXSTYLE, windowedExStyle);

        SetWindowPos(windowHandle, HWND_NOTOPMOST, windowedRect.left, windowedRect.top,
                     windowedRect.right - windowedRect.left, windowedRect.bottom - windowedRect.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        isFullscreen = false;

        // Update button text
        if (fullscreenButton) {
            fullscreenButton->SetContent("FULLSCREEN");
        }
    }
}

void NoesisRenderPath::Start() {
    // Call parent Start first
    RenderPath3D::Start();
    setOutlineEnabled(true);

    // Initialize audio system
    wi::audio::Initialize();

    InitializeNoesis();
    LoadConfig();
    // Note: Animation library is loaded in LoadGameScene() after scene.Clear()
}

void NoesisRenderPath::Stop() {
    // Cleanup Noesis
    ShutdownNoesis();

    // Call parent Stop
    RenderPath3D::Stop();
}

void NoesisRenderPath::Update(float dt) {
    // Call parent Update first
    RenderPath3D::Update(dt);

    // Run shutter animation if active (even in camera mode)
    if (shutterActive) {
        SimulateShutter(dt);
    }

    // Skip walkabout controls while in GUI mode (except camera mode which needs camera control)
    if (inDialogueMode | inCaseboardMode | inMainMenuMode) {
        return;
    }

    // Handle camera mode separately (first-person camera at player's eyes)
    if (inCameraMode && playerCharacter != wi::ecs::INVALID_ENTITY) {
        wi::scene::Scene &scene = wi::scene::GetScene();
        wi::scene::CharacterComponent *playerChar = scene.characters.GetComponent(playerCharacter);

        if (playerChar) {
            // === MOUSE LOOK (same as third-person mode) ===
            if (windowHandle) {
                RECT clientRect;
                GetClientRect(windowHandle, &clientRect);
                POINT center = {(clientRect.right - clientRect.left) / 2,
                                (clientRect.bottom - clientRect.top) / 2};
                POINT currentPos;
                GetCursorPos(&currentPos);
                ScreenToClient(windowHandle, &currentPos);

                if (mouseInitialized) {
                    float deltaX = (float)(currentPos.x - center.x) * mouseSensitivity;
                    float deltaY = (float)(currentPos.y - center.y) * mouseSensitivity;

                    if (deltaX != 0.0f || deltaY != 0.0f) {
                        cameraHorizontal += deltaX;
                        cameraVertical =
                            std::clamp(cameraVertical + deltaY, -XM_PIDIV4, XM_PIDIV4 * 1.2f);
                    }
                } else {
                    mouseInitialized = true;
                }

                POINT screenCenter = center;
                ClientToScreen(windowHandle, &screenCenter);
                SetCursorPos(screenCenter.x, screenCenter.y);
            }

            // === WASD MOVEMENT (first-person) ===
            bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
            bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;
            bool aPressed = (GetAsyncKeyState('A') & 0x8000) != 0;
            bool dPressed = (GetAsyncKeyState('D') & 0x8000) != 0;

            if (wPressed || sPressed || aPressed || dPressed) {
                if (playerChar->IsGrounded() || playerChar->IsWallIntersect()) {
                    // Calculate movement direction based on camera rotation
                    XMFLOAT3 move_dir(0, 0, 0);

                    if (wPressed) {
                        // Forward relative to camera
                        move_dir.x += sinf(cameraHorizontal);
                        move_dir.z += cosf(cameraHorizontal);
                    }
                    if (sPressed) {
                        // Backward relative to camera
                        move_dir.x -= sinf(cameraHorizontal);
                        move_dir.z -= cosf(cameraHorizontal);
                    }
                    if (aPressed) {
                        // Left (strafe) relative to camera
                        move_dir.x -= cosf(cameraHorizontal);
                        move_dir.z += sinf(cameraHorizontal);
                    }
                    if (dPressed) {
                        // Right (strafe) relative to camera
                        move_dir.x += cosf(cameraHorizontal);
                        move_dir.z -= sinf(cameraHorizontal);
                    }

                    // Normalize movement direction
                    float len = sqrtf(move_dir.x * move_dir.x + move_dir.z * move_dir.z);
                    if (len > 0.001f) {
                        move_dir.x /= len;
                        move_dir.z /= len;
                    }
                    move_dir.y = 0.0f;

                    // Walk action with direction
                    auto action =
                        wi::scene::character_system::make_walk(scene, *playerChar, move_dir);
                    playerChar->SetAction(scene, action);
                }
            } else if (playerChar->IsWalking()) {
                // Return to idle when no movement key pressed
                auto action = wi::scene::character_system::make_idle(scene, *playerChar);
                playerChar->SetAction(scene, action);
            }

            // === FIRST-PERSON CAMERA (at player's eyes) ===
            XMFLOAT3 charPos = playerChar->GetPositionInterpolated();

            // Camera position at player's eye level
            XMFLOAT3 camPos;
            camPos.x = charPos.x;
            camPos.y = charPos.y + 1.6f; // Eye level
            camPos.z = charPos.z;

            // Apply camera rotation based on mouse look
            wi::scene::TransformComponent cameraTransform;
            cameraTransform.ClearTransform();
            cameraTransform.Translate(camPos);
            cameraTransform.RotateRollPitchYaw(XMFLOAT3(cameraVertical, cameraHorizontal, 0));
            cameraTransform.UpdateTransform();

            camera->TransformCamera(cameraTransform);
            camera->UpdateCamera();
        }
        return; // Camera mode handled, don't run third-person logic
    }

    // Handle walkabout-style controls and third-person camera when game is active
    if (playerCharacter != wi::ecs::INVALID_ENTITY) {
        wi::scene::Scene &scene = wi::scene::GetScene();
        wi::scene::CharacterComponent *playerChar = scene.characters.GetComponent(playerCharacter);

        if (playerChar && isFirstPersonMode) {
            // ===== MOUSE LOOK (Action System) =====
            if (windowHandle) {
                // Get window center
                RECT clientRect;
                GetClientRect(windowHandle, &clientRect);
                POINT center = {(clientRect.right - clientRect.left) / 2,
                                (clientRect.bottom - clientRect.top) / 2};

                // Get current cursor position
                POINT currentPos;
                GetCursorPos(&currentPos);
                ScreenToClient(windowHandle, &currentPos);

                if (mouseInitialized) {
                    // Calculate mouse delta
                    float deltaX = (float)(currentPos.x - center.x) * mouseSensitivity;
                    float deltaY = (float)(currentPos.y - center.y) * mouseSensitivity;

                    if (deltaX != 0.0f || deltaY != 0.0f) {
                        // Apply mouse look to camera angles
                        cameraHorizontal += deltaX;
                        cameraVertical =
                            std::clamp(cameraVertical + deltaY, -XM_PIDIV4, XM_PIDIV4 * 1.2f);
                    }
                } else {
                    mouseInitialized = true;
                }

                // Reset cursor to center for continuous movement
                POINT screenCenter = center;
                ClientToScreen(windowHandle, &screenCenter);
                SetCursorPos(screenCenter.x, screenCenter.y);
            }

            // Camera zoom with mouse wheel
            XMFLOAT4 pointer = wi::input::GetPointer();
            cameraDistance = std::max(0.5f, cameraDistance - pointer.z * 0.3f);

            // === ACTION SYSTEM: BODY MOTOR - WALK ===
            bool wPressed = (GetAsyncKeyState('W') & 0x8000) != 0;
            bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;

            if (wPressed || sPressed) {
                if (playerChar->IsGrounded() || playerChar->IsWallIntersect()) {
                    // Calculate walk direction based on camera and input
                    XMFLOAT3 walk_dir;
                    if (sPressed && !wPressed) {
                        // S key: walk toward camera (backward)
                        walk_dir.x = -sinf(cameraHorizontal);
                        walk_dir.y = 0.0f;
                        walk_dir.z = -cosf(cameraHorizontal);
                    } else {
                        // W key: walk away from camera (forward)
                        walk_dir.x = sinf(cameraHorizontal);
                        walk_dir.y = 0.0f;
                        walk_dir.z = cosf(cameraHorizontal);
                    }

                    // Walk action with direction (handles body turning internally)
                    auto action =
                        wi::scene::character_system::make_walk(scene, *playerChar, walk_dir);
                    playerChar->SetAction(scene, action);
                }
            } else if (playerChar->IsWalking()) {
                // Return to idle when no movement key pressed
                auto action = wi::scene::character_system::make_idle(scene, *playerChar);
                playerChar->SetAction(scene, action);
            }

            // === ACTION SYSTEM: BODY MOTOR - JUMP ===
            static bool spaceWasPressed = false;
            bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (spacePressed && !spaceWasPressed && playerChar->IsGrounded()) {
                auto action = wi::scene::character_system::make_jump(scene, *playerChar, 8.0f);
                playerChar->SetAction(scene, action);
            }
            spaceWasPressed = spacePressed;

            // Escape key - return to menu
            static bool escWasPressed = false;
            bool escPressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (escPressed && !escWasPressed) {
                SetThirdPersonMode(false);
                inMainMenuMode = true;
                if (menuContainer) {
                    menuContainer->SetVisibility(Noesis::Visibility_Visible);
                }
                CleanupNPCScripts();
                aimDotVisible = false;
                LoadAndPlayMenuMusic();
                wi::backlog::post("Returned to menu (Escape pressed)\n");
            }
            escWasPressed = escPressed;

            // === CAMERA (direct control, no actions) ===
            // Camera orbits around player based on mouse input
            XMFLOAT3 charPos = playerChar->GetPositionInterpolated();

            // Calculate camera's right vector (perpendicular to look direction, horizontal)
            float camRightX = cosf(cameraHorizontal); // Right is 90 degrees from forward
            float camRightZ = -sinf(cameraHorizontal);

            // Calculate camera position orbiting around character
            float camOffsetX = sinf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;
            float camOffsetY = sinf(cameraVertical) * cameraDistance;
            float camOffsetZ = cosf(cameraHorizontal) * cosf(cameraVertical) * cameraDistance;

            // Eye target (what camera looks at) - offset horizontally for over-the-shoulder
            XMFLOAT3 eyeTarget;
            eyeTarget.x = charPos.x + camRightX * cameraHorizontalOffset;
            eyeTarget.y = charPos.y + 1.6f; // Eye level
            eyeTarget.z = charPos.z + camRightZ * cameraHorizontalOffset;

            // Camera position - also offset horizontally
            XMFLOAT3 camPos;
            camPos.x = eyeTarget.x - camOffsetX;
            camPos.y = eyeTarget.y + camOffsetY;
            camPos.z = eyeTarget.z - camOffsetZ;

            // Camera collision check (prevent camera from going through walls)
            XMVECTOR targetPos = XMLoadFloat3(&eyeTarget);
            XMVECTOR cameraPos = XMLoadFloat3(&camPos);

            XMFLOAT3 rayDir;
            XMStoreFloat3(&rayDir, XMVector3Normalize(cameraPos - targetPos));
            float rayDist = XMVectorGetX(XMVector3Length(cameraPos - targetPos));

            wi::primitive::Ray cameraRay(eyeTarget, rayDir, 0.1f, rayDist);
            auto collision = scene.Intersects(
                cameraRay, wi::enums::FILTER_NAVIGATION_MESH | wi::enums::FILTER_COLLIDER, ~1u);

            if (collision.entity != wi::ecs::INVALID_ENTITY && collision.distance < rayDist) {
                XMVECTOR collisionOffset = XMLoadFloat3(&rayDir) * (collision.distance - 0.2f);
                cameraPos = targetPos + collisionOffset;
                XMStoreFloat3(&camPos, cameraPos);
            }

            // Apply camera transform directly (no action system)
            wi::scene::TransformComponent cameraTransform;
            cameraTransform.ClearTransform();
            cameraTransform.Translate(camPos);
            cameraTransform.RotateRollPitchYaw(XMFLOAT3(cameraVertical, cameraHorizontal, 0));
            cameraTransform.UpdateTransform();

            camera->TransformCamera(cameraTransform);
            camera->UpdateCamera();

            // === AIM DOT & NPC DETECTION ===
            aimDotVisible = false;
            aimingAtNPC = false;

            // Aim dot fixed at screen center
            aimDotScreenPos.x = GetLogicalWidth() * 0.5f;
            aimDotScreenPos.y = GetLogicalHeight() * 0.5f;
            aimDotVisible = true;

            // Ray from camera in camera's look direction (same as through screen center)
            XMFLOAT3 camLookDir;
            XMStoreFloat3(&camLookDir, camera->GetAt());

            // Raycast from camera in look direction
            wi::primitive::Ray aimRay(camPos, camLookDir, 0.1f, 100.0f);
            auto aimHit = scene.Intersects(
                aimRay, wi::enums::FILTER_OPAQUE | wi::enums::FILTER_TRANSPARENT, ~0u);

            // Calculate target point (hit or 100 units out)
            XMFLOAT3 targetPoint;
            if (aimHit.entity != wi::ecs::INVALID_ENTITY) {
                targetPoint.x = camPos.x + camLookDir.x * aimHit.distance;
                targetPoint.y = camPos.y + camLookDir.y * aimHit.distance;
                targetPoint.z = camPos.z + camLookDir.z * aimHit.distance;
            } else {
                targetPoint.x = camPos.x + camLookDir.x * 100.0f;
                targetPoint.y = camPos.y + camLookDir.y * 100.0f;
                targetPoint.z = camPos.z + camLookDir.z * 100.0f;
            }

            // Draw debug line (transparent green)
            wi::renderer::RenderableLine debugLine;
            debugLine.start = camPos;
            debugLine.end = targetPoint;
            debugLine.color_start = XMFLOAT4(0, 1, 0, 0.3f); // Green, transparent
            debugLine.color_end = XMFLOAT4(0, 1, 0, 0.3f);
            wi::renderer::DrawLine(debugLine);

            // Check if we hit an NPC
            aimedNPCEntity = wi::ecs::INVALID_ENTITY;
            if (aimHit.entity != wi::ecs::INVALID_ENTITY) {
                // Check if hit entity or any ancestor is an NPC
                wi::ecs::Entity checkEntity = aimHit.entity;
                for (int depth = 0; depth < 10 && checkEntity != wi::ecs::INVALID_ENTITY; ++depth) {
                    // Check if this entity has a MindComponent with NPC type
                    const wi::scene::MindComponent *mind = scene.minds.GetComponent(checkEntity);
                    if (mind != nullptr && mind->IsNPC()) {
                        aimingAtNPC = true;
                        aimedNPCEntity = checkEntity; // Store the NPC entity
                        break;
                    }
                    // Check parent via hierarchy
                    const wi::scene::HierarchyComponent *hier =
                        scene.hierarchy.GetComponent(checkEntity);
                    if (hier != nullptr) {
                        checkEntity = hier->parentID;
                    } else {
                        break;
                    }
                }
            }

            // Update Noesis Talk indicator visibility (only during gameplay)
            if (talkIndicator) {
                talkIndicator->SetVisibility((!inMainMenuMode && aimingAtNPC)
                                                 ? Noesis::Visibility_Visible
                                                 : Noesis::Visibility_Collapsed);
            }
        }

        // Update NPC behavior via Lua scripts (like walkabout)
        if ((patrolScriptLoaded || guardScriptLoaded) && !npcEntities.empty()) {
            for (auto npc : npcEntities) {
                if (npc == wi::ecs::INVALID_ENTITY) {
                    continue;
                }
                // Get the MindComponent to determine which script callback to use
                const wi::scene::MindComponent *mind = scene.minds.GetComponent(npc);
                if (mind == nullptr || mind->scriptCallback.empty()) {
                    continue;
                }
                // Call the appropriate Lua update function for each NPC
                // e.g., npc_patrol_update(entity, dt) or npc_guard_update(entity, dt)
                std::string lua_call = mind->scriptCallback + "(" + std::to_string(npc) + ", " +
                                       std::to_string(dt) + ")";
                wi::lua::RunText(lua_call);
            }
        }
    }
}

void NoesisRenderPath::PreRender() {
    // Call parent PreRender first (sets up Wicked Engine rendering)
    RenderPath3D::PreRender();

    // Noesis offscreen rendering before main render target is bound
    if (uiView && noesisDevice) {
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        wi::graphics::CommandList cmd = device->BeginCommandList();

        // Get D3D12 device and command list
        wi::graphics::GraphicsDevice_DX12 *dx12Device =
            static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
        ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

        // Get safe fence value (frame count + buffer count for safety)
        uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;

        // Set command list for Noesis (MUST be called before any Noesis rendering)
        NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

        // Update Noesis view
        if (startTime == 0) {
            startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        }
        uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();
        double time = (currentTime - startTime) / 1000.0;
        uiView->Update(time);

        // Update render tree and render offscreen textures
        // IMPORTANT: Do this BEFORE Wicked Engine binds its main render target!
        uiView->GetRenderer()->UpdateRenderTree();
        uiView->GetRenderer()->RenderOffscreen();
    }
}

// Called after Render() but before Compose() - the render target is now filled
void NoesisRenderPath::PostRender() {
    RenderPath3D::PostRender();

    // If a capture was requested, do it now (after rendering is complete)
    if (captureRequestPending) {
        captureRequestPending = false;

        // IMPORTANT: Force submit all pending command lists and wait for GPU
        // The rendering command list hasn't been submitted yet!
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        device->SubmitCommandLists();
        device->WaitForGPU();

        const_cast<NoesisRenderPath *>(this)->CaptureFrameToMemory();
    }
}

void NoesisRenderPath::Compose(wi::graphics::CommandList cmd) const {
    // Call parent Compose first (Wicked Engine composes its layers to backbuffer)
    RenderPath3D::Compose(cmd);

    // Draw aim dot at raycast hit position (not in camera mode)
    if (!inMainMenuMode && !inCameraMode && aimDotVisible) {
        wi::image::SetCanvas(*this);

        // Outer circle: 4px radius, semi-transparent
        {
            wi::image::Params params;
            params.pos = XMFLOAT3(aimDotScreenPos.x - 4.0f, aimDotScreenPos.y - 4.0f, 0);
            params.siz = XMFLOAT2(8.0f, 8.0f);
            params.pivot = XMFLOAT2(0, 0);
            params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f); // White, 30% opacity
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            params.corners_rounding[0] = {4.0f, 8}; // Top-left
            params.corners_rounding[1] = {4.0f, 8}; // Top-right
            params.corners_rounding[2] = {4.0f, 8}; // Bottom-left
            params.corners_rounding[3] = {4.0f, 8}; // Bottom-right
            wi::image::Draw(nullptr, params, cmd);
        }

        // Inner dot: 2px radius, fully opaque
        {
            wi::image::Params params;
            params.pos = XMFLOAT3(aimDotScreenPos.x - 2.0f, aimDotScreenPos.y - 2.0f, 0);
            params.siz = XMFLOAT2(4.0f, 4.0f);
            params.pivot = XMFLOAT2(0, 0);
            params.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // White, fully opaque
            params.blendFlag = wi::enums::BLENDMODE_ALPHA;
            params.enableCornerRounding();
            params.corners_rounding[0] = {2.0f, 8}; // Top-left
            params.corners_rounding[1] = {2.0f, 8}; // Top-right
            params.corners_rounding[2] = {2.0f, 8}; // Bottom-left
            params.corners_rounding[3] = {2.0f, 8}; // Bottom-right
            wi::image::Draw(nullptr, params, cmd);
        }
    }

    // Render Noesis UI on top of everything
    if (uiView && noesisDevice) {
        wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
        wi::graphics::GraphicsDevice_DX12 *dx12Device =
            static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
        ID3D12GraphicsCommandList *d3d12CmdList = dx12Device->GetD3D12CommandList(cmd);

        // At this point, Wicked Engine has:
        // 1. Rendered the 3D scene to offscreen textures
        // 2. Composed those textures to the backbuffer
        // 3. The backbuffer is bound as render target

        // IMPORTANT: Set command list for Noesis before rendering
        // Get safe fence value (frame count + buffer count for safety)
        uint64_t safeFenceValue = device->GetFrameCount() + device->GetBufferCount() + 1;
        NoesisApp::D3D12Factory::SetCommandList(noesisDevice, d3d12CmdList, safeFenceValue);

        // Render Noesis UI on top
        uiView->GetRenderer()->Render();

        // Complete any pending split barriers
        NoesisApp::D3D12Factory::EndPendingSplitBarriers(noesisDevice);
    }
}

void NoesisRenderPath::ResizeBuffers() {
    // Call parent ResizeBuffers first
    RenderPath3D::ResizeBuffers();

    // Update Noesis view size
    if (uiView) {
        XMUINT2 internalResolution = GetInternalResolution();
        uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);
    }
}

bool NoesisRenderPath::MouseWheel(int x, int y, int delta) {
    if (!uiView)
        return false;

    // Handle caseboard zoom
    if (inCaseboardMode) {
        CaseboardZoom(x, y, (float)delta);
        return true;
    }

    // Forward to Noesis
    return uiView->MouseWheel(x, y, delta);
}

void NoesisRenderPath::InitializeNoesis() {
    // Initialize Noesis
    Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);
    Noesis::GUI::Init();

    // Set texture provider to load images from GUI folder
    Noesis::GUI::SetTextureProvider(Noesis::MakePtr<NoesisApp::LocalTextureProvider>("."));

    Noesis::GUI::SetXamlProvider(Noesis::MakePtr<NoesisApp::LocalXamlProvider>("./GUI"));

    Noesis::GUI::SetFontProvider(Noesis::MakePtr<NoesisApp::LocalFontProvider>("./GUI"));
    const char *fonts[] = {"Noesis/Data/Theme/Fonts/#PT Root UI", "Arial", "Segoe UI Emoji"};
    Noesis::GUI::SetFontFallbacks(fonts, 3);
    Noesis::GUI::SetFontDefaultProperties(14.0f, Noesis::FontWeight_Normal,
                                          Noesis::FontStretch_Normal, Noesis::FontStyle_Normal);

    Noesis::GUI::LoadApplicationResources("Noesis/Data/Theme/NoesisTheme.DarkBlue.xaml");

    Noesis::Uri panelUri("Panel.xaml");
    Noesis::Ptr<Noesis::FrameworkElement> panelRoot =
        Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(panelUri);
    if (!panelRoot) {
        wi::backlog::post("ERROR: Failed to load Panel.xaml\n");
        return;
    }

    wi::backlog::post("Successfully loaded Panel.xaml\n");
    rootElement = panelRoot;

    // Find all UI elements by name from the XAML
    Noesis::Grid *rootGrid = Noesis::DynamicCast<Noesis::Grid *>(panelRoot.GetPtr());
    if (!rootGrid) {
        return;
    }

    // Find main menu UI elements
    menuContainer = FindElementByName<Noesis::Grid>(rootGrid, "MenuContainer");
    seedTextBox = FindElementByName<Noesis::TextBox>(rootGrid, "SeedTextBox");
    playGameButton = FindElementByName<Noesis::Button>(rootGrid, "PlayGameButton");
    fullscreenButton = FindElementByName<Noesis::Button>(rootGrid, "FullscreenButton");
    talkIndicator = FindElementByName<Noesis::FrameworkElement>(rootGrid, "TalkIndicator");

    // Find dialogue panel UI elements
    dialoguePanelRoot = FindElementByName<Noesis::Grid>(rootGrid, "DialoguePanelRoot");
    dialogueScrollViewer =
        FindElementByName<Noesis::ScrollViewer>(rootGrid, "DialogueScrollViewer");
    dialogueList = FindElementByName<Noesis::StackPanel>(rootGrid, "DialogueList");
    dialogueInput = FindElementByName<Noesis::TextBox>(rootGrid, "DialogueInput");
    dialogueHintText = FindElementByName<Noesis::TextBlock>(rootGrid, "DialogueHintText");

    // Find caseboard panel UI elements
    caseboardPanel = FindElementByName<Noesis::Grid>(rootGrid, "CaseboardPanel");
    caseboardDebugText = FindElementByName<Noesis::TextBlock>(rootGrid, "CaseboardDebugText");

    // Find camera panel UI elements
    cameraPanel = FindElementByName<Noesis::Grid>(rootGrid, "CameraPanel");
    shutterBarTop = FindElementByName<Noesis::FrameworkElement>(rootGrid, "ShutterBarTop");
    shutterBarBottom = FindElementByName<Noesis::FrameworkElement>(rootGrid, "ShutterBarBottom");
    cameraPhotoCount = FindElementByName<Noesis::TextBlock>(rootGrid, "CameraPhotoCount");

    // Find the CaseboardContent canvas and get its transforms
    caseboardContent = FindElementByName<Noesis::Panel>(rootGrid, "CaseboardContent");
    if (caseboardContent) {
        Noesis::Transform *transform = caseboardContent->GetRenderTransform();
        if (transform) {
            Noesis::TransformGroup *transformGroup =
                Noesis::DynamicCast<Noesis::TransformGroup *>(transform);
            if (transformGroup && transformGroup->GetNumChildren() >= 2) {
                caseboardZoomTransform =
                    Noesis::DynamicCast<Noesis::ScaleTransform *>(transformGroup->GetChild(0));
                caseboardPanTransform =
                    Noesis::DynamicCast<Noesis::TranslateTransform *>(transformGroup->GetChild(1));
            }
        }
    }

    // Wire up event handlers
    if (playGameButton) {
        playGameButton->Click() +=
            [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                // Get seed value
                const char *seedText = seedTextBox ? seedTextBox->GetText() : "12345";

                char buffer[512];
                sprintf_s(buffer, "Starting game with seed: %s\n", seedText);
                wi::backlog::post(buffer);

                // Hide the menu
                inMainMenuMode = false;
                if (menuContainer) {
                    menuContainer->SetVisibility(Noesis::Visibility_Collapsed);
                }

                // Stop menu music
                if (menuMusicInstance.IsValid()) {
                    wi::audio::Stop(&menuMusicInstance);
                    menuMusicInstance = {};
                    menuMusic = {};
                }

                // Load the scene
                LoadGameScene();

                // Enable walkabout controls (mouse capture for third-person camera control)
                SetThirdPersonMode(true);
            };
    }

    // Wire up fullscreen button
    if (fullscreenButton) {
        fullscreenButton->Click() +=
            [this](Noesis::BaseComponent *sender, const Noesis::RoutedEventArgs &args) {
                ToggleFullscreen();
            };
    }

    // Initial control state update
    UpdateControlStates();

    // Create UserControl to wrap everything
    Noesis::Ptr<Noesis::UserControl> root = Noesis::MakePtr<Noesis::UserControl>();
    root->SetContent(panelRoot);

    // Transparent background
    Noesis::Ptr<Noesis::SolidColorBrush> transparentBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>();
    transparentBrush->SetColor(Noesis::Color(0, 0, 0, 0));
    root->SetBackground(transparentBrush);

    uiView = Noesis::GUI::CreateView(root);

    // Get Wicked Engine's D3D12 device
    wi::graphics::GraphicsDevice *device = wi::graphics::GetDevice();
    wi::graphics::GraphicsDevice_DX12 *dx12Device =
        static_cast<wi::graphics::GraphicsDevice_DX12 *>(device);
    ID3D12Device *d3d12Device = dx12Device->GetD3D12Device();

    // Create fence for Noesis
    HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence));
    if (FAILED(hr)) {
        frameFence = nullptr;
    }

    // Get format info from Wicked Engine's swap chain
    DXGI_SAMPLE_DESC sampleDesc = {1, 0};
    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT stencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    bool sRGB = false;

    // Create Noesis device from Wicked Engine's device
    noesisDevice = NoesisApp::D3D12Factory::CreateDevice(d3d12Device, frameFence, colorFormat,
                                                         stencilFormat, sampleDesc, sRGB);

    // Initialize renderer
    uiView->GetRenderer()->Init(noesisDevice);

    // Get window size from Wicked Engine
    XMUINT2 internalResolution = GetInternalResolution();
    uiView->SetSize((float)internalResolution.x, (float)internalResolution.y);

    // Update render tree
    uiView->GetRenderer()->UpdateRenderTree();
}

void NoesisRenderPath::ShutdownNoesis() {
    // Stop music
    if (menuMusicInstance.IsValid()) {
        wi::audio::Stop(&menuMusicInstance);
    }

    if (uiView) {
        uiView->GetRenderer()->Shutdown();
    }

    uiView.Reset();
    noesisDevice.Reset();
    seedTextBox.Reset();
    playGameButton.Reset();
    fullscreenButton.Reset();
    menuContainer.Reset();
    talkIndicator.Reset();
    dialoguePanelRoot.Reset();
    dialogueScrollViewer.Reset();
    dialogueList.Reset();
    dialogueInput.Reset();
    dialogueHintText.Reset();
    caseboardPanel.Reset();
    caseboardContent.Reset();
    caseboardDebugText.Reset();
    caseboardZoomTransform = nullptr;
    caseboardPanTransform = nullptr;
    noteCards.clear();
    cameraPanel.Reset();
    shutterBarTop.Reset();
    shutterBarBottom.Reset();
    cameraPhotoCount.Reset();
    photoCards.clear();
    capturedPhotoTextures.clear();
    rootElement.Reset();

    if (frameFence) {
        frameFence->Release();
        frameFence = nullptr;
    }

    Noesis::GUI::Shutdown();
}
