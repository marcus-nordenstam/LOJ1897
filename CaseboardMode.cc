#include "CaseboardMode.h"

#include <NsDrawing/Color.h>
#include <NsDrawing/CornerRadius.h>
#include <NsDrawing/Thickness.h>
#include <NsGui/BitmapImage.h>
#include <NsGui/Border.h>
#include <NsGui/Canvas.h>
#include <NsGui/DropShadowEffect.h>
#include <NsGui/Enums.h>
#include <NsGui/FontFamily.h>
#include <NsGui/FontProperties.h>
#include <NsGui/Grid.h>
#include <NsGui/Image.h>
#include <NsGui/RowDefinition.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/TextProperties.h>
#include <NsGui/TransformGroup.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/Uri.h>

#include <algorithm>

void CaseboardMode::Initialize(Noesis::Grid *panel, Noesis::Panel *content,
                               Noesis::TextBlock *debugText, HWND hwnd) {
    caseboardPanel = Noesis::Ptr<Noesis::Grid>(panel);
    caseboardContent = Noesis::Ptr<Noesis::Panel>(content);
    caseboardDebugText = Noesis::Ptr<Noesis::TextBlock>(debugText);
    windowHandle = hwnd;

    // Get transforms from the content panel
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
}

void CaseboardMode::Shutdown() {
    caseboardPanel.Reset();
    caseboardContent.Reset();
    caseboardDebugText.Reset();
    caseboardZoomTransform = nullptr;
    caseboardPanTransform = nullptr;
    noteCards.clear();
    photoCards.clear();
    capturedPhotoTextures.clear();
}

void CaseboardMode::EnterCaseboardMode() {
    if (inCaseboardMode)
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
        if (viewportHoriz > viewportVert) {
            caseboardVisibleHalfY = 3000.0f;
            caseboardVisibleHalfX = 3000.0f * (viewportHoriz / viewportVert);
        } else {
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

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(true);
    }

    wi::backlog::post("Entered caseboard mode\n");
}

void CaseboardMode::ExitCaseboardMode() {
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

    // Notify callback
    if (modeChangeCallback) {
        modeChangeCallback(false);
    }

    wi::backlog::post("Exited caseboard mode\n");
}

void CaseboardMode::UpdateCaseboardTransforms() {
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

void CaseboardMode::UpdateCaseboardDebugText() {
    if (!caseboardDebugText)
        return;

    // Convert screen mouse position to board space
    float boardX = (caseboardCurrentMousePos.x - caseboardPanX) / caseboardZoom;
    float boardY = (caseboardCurrentMousePos.y - caseboardPanY) / caseboardZoom;

    // Format debug text showing position and visible area bounds
    char debugStr[256];
    snprintf(debugStr, sizeof(debugStr),
             "Board: (%.0f, %.0f)  Zoom: %.2fx  Visible: +/-%.0f x +/-%.0f", boardX, boardY,
             caseboardZoom, caseboardVisibleHalfX, caseboardVisibleHalfY);
    caseboardDebugText->SetText(debugStr);
}

void CaseboardMode::CaseboardZoom(int x, int y, float delta) {
    if (!inCaseboardMode)
        return;

    float mouseX = (float)x;
    float mouseY = (float)y;

    // Calculate world position under mouse before zoom
    float worldXBefore = (mouseX - caseboardPanX) / caseboardZoom;
    float worldYBefore = (mouseY - caseboardPanY) / caseboardZoom;

    // Apply zoom
    float zoomDelta = delta * 0.001f;
    caseboardZoom = std::clamp(caseboardZoom + zoomDelta, 0.32f, 4.0f);

    // Calculate world position under mouse after zoom
    float worldXAfter = (mouseX - caseboardPanX) / caseboardZoom;
    float worldYAfter = (mouseY - caseboardPanY) / caseboardZoom;

    // Adjust pan to keep mouse position stable
    caseboardPanX += (worldXAfter - worldXBefore) * caseboardZoom;
    caseboardPanY += (worldYAfter - worldYBefore) * caseboardZoom;

    ClampCaseboardPan();
    UpdateCaseboardTransforms();
}

void CaseboardMode::CaseboardPanStart(int x, int y) {
    if (!inCaseboardMode)
        return;

    // Convert click position to board space
    float boardClickX = (x - caseboardPanX) / caseboardZoom;
    float boardClickY = (y - caseboardPanY) / caseboardZoom;

    // If we're editing a note and click elsewhere, finalize the edit
    if (editingNoteCardIndex >= 0 && editingNoteCardIndex < (int)noteCards.size()) {
        NoteCard &card = noteCards[editingNoteCardIndex];

        float cardLeft = card.boardX - 90.0f;
        float cardTop = card.boardY - 116.0f;
        float cardRight = cardLeft + 180.0f;
        float cardBottom = cardTop + 232.0f;

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
                continue;

            float cardLeft = card.boardX - 90.0f + 15.0f;
            float cardTop = card.boardY - 116.0f + 40.0f;
            float cardRight = card.boardX - 90.0f + 180.0f - 15.0f;
            float cardBottom = card.boardY - 116.0f + 232.0f - 20.0f;

            if (boardClickX >= cardLeft && boardClickX <= cardRight && boardClickY >= cardTop &&
                boardClickY <= cardBottom) {
                StartEditingNoteCard(i);
                return;
            }
        }
    }

    // Check if clicking on a note card's drag area
    int dragCardIndex = HitTestNoteCardDragArea(boardClickX, boardClickY);
    if (dragCardIndex >= 0) {
        StartDraggingNoteCard(dragCardIndex, boardClickX, boardClickY);
        return;
    }

    // Check if clicking on a photo card
    int dragPhotoIndex = HitTestPhotoCard(boardClickX, boardClickY);
    if (dragPhotoIndex >= 0) {
        StartDraggingPhotoCard(dragPhotoIndex, boardClickX, boardClickY);
        return;
    }

    // Check if clicking on a testimony card
    int dragTestimonyIndex = HitTestTestimonyCard(boardClickX, boardClickY);
    if (dragTestimonyIndex >= 0) {
        StartDraggingTestimonyCard(dragTestimonyIndex, boardClickX, boardClickY);
        return;
    }

    // Check if clicking on a case-file
    int caseFileIndex = HitTestCaseFile(boardClickX, boardClickY);
    if (caseFileIndex >= 0) {
        CaseFile &file = caseFiles[caseFileIndex];
        float fileLeft = file.boardX - file.width / 2.0f;
        float fileTop = file.boardY - file.height / 2.0f;
        float localX = boardClickX - fileLeft;
        float localY = boardClickY - fileTop;

        // Check if clicking on the right tab (forward)
        float rightTabStartX = file.width - 20.5f; // Tab starts at this X position (tabWidth + margin)
        // Check if clicking on the left tab (backward) - only visible when currentPage > 0
        float leftTabEndX = 20.5f; // Left tab extends to this X position

        if (localX >= rightTabStartX) {
            // Clicked on right tab - navigate to next page
            HandleCaseFileClick(caseFileIndex, localX, localY);
        } else if (localX <= leftTabEndX && file.currentPage > 0) {
            // Clicked on left tab - navigate to previous page
            HandleCaseFileLeftTabClick(caseFileIndex);
        } else {
            // Clicked on main area - start dragging
            StartDraggingCaseFile(caseFileIndex, boardClickX, boardClickY);
        }
        return;
    }

    caseboardPanning = true;
    caseboardLastMousePos.x = x;
    caseboardLastMousePos.y = y;
}

void CaseboardMode::CaseboardPanEnd() {
    caseboardPanning = false;
    StopDraggingNoteCard();
    StopDraggingPhotoCard();
    StopDraggingTestimonyCard();
    StopDraggingCaseFile();
}

void CaseboardMode::ClampCaseboardPan() {
    if (!windowHandle)
        return;

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);

    float viewportHoriz = (float)(clientRect.right - clientRect.left);
    float viewportVert = (float)(clientRect.bottom - clientRect.top);

    float maxPanX = caseboardVisibleHalfX * caseboardZoom;
    float minPanX = viewportHoriz - caseboardVisibleHalfX * caseboardZoom;

    float maxPanY = caseboardVisibleHalfY * caseboardZoom;
    float minPanY = viewportVert - caseboardVisibleHalfY * caseboardZoom;

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

void CaseboardMode::CaseboardPanMove(int x, int y) {
    if (!inCaseboardMode)
        return;

    caseboardCurrentMousePos.x = x;
    caseboardCurrentMousePos.y = y;

    float boardX = (x - caseboardPanX) / caseboardZoom;
    float boardY = (y - caseboardPanY) / caseboardZoom;

    // Handle note card dragging
    if (IsDraggingNoteCard()) {
        UpdateDraggingNoteCard(boardX, boardY);
        UpdateCaseboardDebugText();
        return;
    }

    // Handle photo card dragging
    if (IsDraggingPhotoCard()) {
        UpdateDraggingPhotoCard(boardX, boardY);
        UpdateCaseboardDebugText();
        return;
    }

    // Handle testimony card dragging
    if (IsDraggingTestimonyCard()) {
        UpdateDraggingTestimonyCard(boardX, boardY);
        UpdateCaseboardDebugText();
        return;
    }

    // Handle case-file dragging
    if (IsDraggingCaseFile()) {
        UpdateDraggingCaseFile(boardX, boardY);
        UpdateCaseboardDebugText();
        return;
    }

    if (caseboardPanning) {
        float deltaX = (float)(x - caseboardLastMousePos.x);
        float deltaY = (float)(y - caseboardLastMousePos.y);

        caseboardPanX += deltaX;
        caseboardPanY += deltaY;

        ClampCaseboardPan();

        caseboardLastMousePos.x = x;
        caseboardLastMousePos.y = y;

        UpdateCaseboardTransforms();
    } else {
        // Check if hovering over a note card drag area, photo card, or case-file for cursor change
        int hoverCardIndex = HitTestNoteCardDragArea(boardX, boardY);
        int hoverPhotoIndex = HitTestPhotoCard(boardX, boardY);
        int hoverCaseFileIndex = HitTestCaseFile(boardX, boardY);
        if ((hoverCardIndex >= 0 || hoverPhotoIndex >= 0 || hoverCaseFileIndex >= 0) &&
            windowHandle) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
        }

        UpdateCaseboardDebugText();
    }
}

void CaseboardMode::AddNoteCard() {
    if (!inCaseboardMode || !caseboardContent)
        return;

    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);
    float viewCenterX = (float)(clientRect.right - clientRect.left) / 2.0f;
    float viewCenterY = (float)(clientRect.bottom - clientRect.top) / 2.0f;

    float boardX = (viewCenterX - caseboardPanX) / caseboardZoom;
    float boardY = (viewCenterY - caseboardPanY) / caseboardZoom;

    // Create the note card container (same dimensions as black cards)
    Noesis::Ptr<Noesis::Grid> noteContainer = *new Noesis::Grid();
    noteContainer->SetWidth(180.0f);
    noteContainer->SetHeight(232.0f);

    // Create background image
    Noesis::Ptr<Noesis::Image> bgImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> bitmapSource = *new Noesis::BitmapImage();
    bitmapSource->SetUriSource(Noesis::Uri("GUI/Cards/Note_Card.png"));
    bgImage->SetSource(bitmapSource);
    bgImage->SetStretch(Noesis::Stretch_Fill);
    noteContainer->GetChildren()->Add(bgImage);

    // Create text box for note input
    Noesis::Ptr<Noesis::TextBox> textBox = *new Noesis::TextBox();
    textBox->SetText("");
    textBox->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textBox->SetAcceptsReturn(true);
    textBox->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    textBox->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    textBox->SetMargin(Noesis::Thickness(15.0f, 20.0f, 15.0f, 20.0f));
    textBox->SetFontSize(14.0f);
    textBox->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));

    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textBox->SetForeground(inkBrush);
    textBox->SetBackground(nullptr);
    textBox->SetBorderThickness(Noesis::Thickness(0.0f));
    textBox->SetCaretBrush(inkBrush);
    noteContainer->GetChildren()->Add(textBox);

    // Position on canvas (centered)
    Noesis::Canvas::SetLeft(noteContainer, boardX - 90.0f);
    Noesis::Canvas::SetTop(noteContainer, boardY - 116.0f);

    // Add to caseboard
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(noteContainer);
    }

    // Store reference
    NoteCard card;
    card.container = noteContainer;
    card.textBox = textBox;
    card.textLabel = nullptr;
    card.boardX = boardX;
    card.boardY = boardY;
    card.isEditing = true;
    card.savedText = "";
    noteCards.push_back(card);

    editingNoteCardIndex = (int)noteCards.size() - 1;
    textBox->Focus();

    char buf[128];
    sprintf_s(buf, "Added note card at board position (%.0f, %.0f)\n", boardX, boardY);
    wi::backlog::post(buf);
}

void CaseboardMode::FinalizeNoteCardEdit() {
    if (editingNoteCardIndex < 0 || editingNoteCardIndex >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[editingNoteCardIndex];
    if (!card.isEditing || !card.textBox)
        return;

    const char *text = card.textBox->GetText();
    card.savedText = text ? text : "";

    Noesis::UIElementCollection *children = card.container->GetChildren();
    if (children) {
        children->Remove(card.textBox.GetPtr());
    }

    // Create TextBlock for display
    Noesis::Ptr<Noesis::TextBlock> textLabel = *new Noesis::TextBlock();
    textLabel->SetText(card.savedText.c_str());
    textLabel->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    textLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Left);
    textLabel->SetMargin(Noesis::Thickness(27.0f, 28.0f, 15.0f, 20.0f));
    textLabel->SetFontSize(14.0f);
    textLabel->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));
    textLabel->SetFontWeight(Noesis::FontWeight_Normal);

    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textLabel->SetForeground(inkBrush);
    textLabel->SetEffect(nullptr);

    if (children) {
        children->Add(textLabel);
    }

    card.textLabel = textLabel;
    card.textBox.Reset();
    card.isEditing = false;

    editingNoteCardIndex = -1;

    wi::backlog::post("Finalized note card edit\n");
}

void CaseboardMode::OnCaseboardClick() {
    if (editingNoteCardIndex >= 0) {
        FinalizeNoteCardEdit();
    }
}

void CaseboardMode::StartEditingNoteCard(int index) {
    if (index < 0 || index >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[index];
    if (card.isEditing)
        return;

    Noesis::UIElementCollection *children = card.container->GetChildren();
    if (children && card.textLabel) {
        children->Remove(card.textLabel.GetPtr());
    }

    Noesis::Ptr<Noesis::TextBox> textBox = *new Noesis::TextBox();
    textBox->SetText(card.savedText.c_str());
    textBox->SetTextWrapping(Noesis::TextWrapping_Wrap);
    textBox->SetAcceptsReturn(true);
    textBox->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    textBox->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    textBox->SetMargin(Noesis::Thickness(15.0f, 20.0f, 15.0f, 20.0f));
    textBox->SetFontSize(16.0f);
    textBox->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));

    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textBox->SetForeground(inkBrush);
    textBox->SetBackground(nullptr);
    textBox->SetBorderThickness(Noesis::Thickness(0.0f));
    textBox->SetCaretBrush(inkBrush);

    if (children) {
        children->Add(textBox);
    }

    card.textBox = textBox;
    card.textLabel.Reset();
    card.isEditing = true;
    editingNoteCardIndex = index;

    textBox->Focus();

    wi::backlog::post("Started editing existing note card\n");
}

int CaseboardMode::HitTestNoteCardDragArea(float boardX, float boardY) {
    for (int i = (int)noteCards.size() - 1; i >= 0; i--) {
        NoteCard &card = noteCards[i];

        float cardLeft = card.boardX - 90.0f;
        float cardTop = card.boardY - 116.0f;
        float cardRight = cardLeft + 180.0f;
        float cardBottom = cardTop + 232.0f;

        float textLeft = cardLeft + 15.0f;
        float textTop = cardTop + 20.0f;
        float textRight = cardRight - 15.0f;
        float textBottom = cardBottom - 20.0f;

        if (boardX >= cardLeft && boardX <= cardRight && boardY >= cardTop &&
            boardY <= cardBottom) {
            bool inTextArea = (boardX >= textLeft && boardX <= textRight && boardY >= textTop &&
                               boardY <= textBottom);
            if (!inTextArea) {
                return i;
            }
        }
    }
    return -1;
}

void CaseboardMode::StartDraggingNoteCard(int index, float boardX, float boardY) {
    if (index < 0 || index >= (int)noteCards.size())
        return;

    if (editingNoteCardIndex == index) {
        FinalizeNoteCardEdit();
    }

    draggingNoteCardIndex = index;
    NoteCard &card = noteCards[index];

    dragOffsetX = boardX - card.boardX;
    dragOffsetY = boardY - card.boardY;

    wi::backlog::post("Started dragging note card\n");
}

void CaseboardMode::UpdateDraggingNoteCard(float boardX, float boardY) {
    if (draggingNoteCardIndex < 0 || draggingNoteCardIndex >= (int)noteCards.size())
        return;

    NoteCard &card = noteCards[draggingNoteCardIndex];

    card.boardX = boardX - dragOffsetX;
    card.boardY = boardY - dragOffsetY;

    if (card.container) {
        Noesis::Canvas::SetLeft(card.container, card.boardX - 90.0f);
        Noesis::Canvas::SetTop(card.container, card.boardY - 110.0f);
    }
}

void CaseboardMode::StopDraggingNoteCard() {
    if (draggingNoteCardIndex >= 0) {
        wi::backlog::post("Stopped dragging note card\n");
    }
    draggingNoteCardIndex = -1;
}

int CaseboardMode::HitTestPhotoCard(float boardX, float boardY) {
    // Check from top to bottom (reverse order for top-most card first)
    for (int i = (int)photoCards.size() - 1; i >= 0; i--) {
        PhotoCard &card = photoCards[i];

        float cardLeft = card.boardX - card.width / 2.0f;
        float cardTop = card.boardY - card.height / 2.0f;
        float cardRight = cardLeft + card.width;
        float cardBottom = cardTop + card.height;

        if (boardX >= cardLeft && boardX <= cardRight && boardY >= cardTop &&
            boardY <= cardBottom) {
            return i;
        }
    }
    return -1;
}

void CaseboardMode::StartDraggingPhotoCard(int index, float boardX, float boardY) {
    if (index < 0 || index >= (int)photoCards.size())
        return;

    draggingPhotoCardIndex = index;
    PhotoCard &card = photoCards[index];

    dragOffsetX = boardX - card.boardX;
    dragOffsetY = boardY - card.boardY;

    wi::backlog::post("Started dragging photo card\n");
}

void CaseboardMode::UpdateDraggingPhotoCard(float boardX, float boardY) {
    if (draggingPhotoCardIndex < 0 || draggingPhotoCardIndex >= (int)photoCards.size())
        return;

    PhotoCard &card = photoCards[draggingPhotoCardIndex];

    card.boardX = boardX - dragOffsetX;
    card.boardY = boardY - dragOffsetY;

    if (card.container) {
        Noesis::Canvas::SetLeft(card.container, card.boardX - card.width / 2.0f);
        Noesis::Canvas::SetTop(card.container, card.boardY - card.height / 2.0f);
    }
}

void CaseboardMode::StopDraggingPhotoCard() {
    if (draggingPhotoCardIndex >= 0) {
        wi::backlog::post("Stopped dragging photo card\n");
    }
    draggingPhotoCardIndex = -1;
}

void CaseboardMode::AddTestimonyCard(const std::string &speaker, const std::string &message) {
    if (!caseboardContent) {
        wi::backlog::post("AddTestimonyCard: caseboardContent is null!\n");
        return;
    }

    wi::backlog::post("AddTestimonyCard: Creating testimony card...\n");

    // Calculate position in board space (offset below existing cards)
    float boardX = 400.0f + testimonyCards.size() * 210.0f;
    float boardY = 400.0f;

    // Create testimony card
    TestimonyCard testimony;
    testimony.speaker = speaker;
    testimony.message = message;
    testimony.boardX = boardX;
    testimony.boardY = boardY;

    // Create container Grid (same dimensions as black cards)
    Noesis::Ptr<Noesis::Grid> cardContainer = Noesis::MakePtr<Noesis::Grid>();
    cardContainer->SetWidth(180.0f);
    cardContainer->SetHeight(232.0f);

    // Add rows for speaker and message
    Noesis::Ptr<Noesis::RowDefinition> row0 = Noesis::MakePtr<Noesis::RowDefinition>();
    row0->SetHeight(Noesis::GridLength(50.0f, Noesis::GridUnitType_Pixel));
    cardContainer->GetRowDefinitions()->Add(row0);
    
    Noesis::Ptr<Noesis::RowDefinition> row1 = Noesis::MakePtr<Noesis::RowDefinition>();
    row1->SetHeight(Noesis::GridLength(1.0f, Noesis::GridUnitType_Star));
    cardContainer->GetRowDefinitions()->Add(row1);

    // Background image (Note_Card.png)
    Noesis::Ptr<Noesis::Image> backgroundImage = Noesis::MakePtr<Noesis::Image>();
    Noesis::Ptr<Noesis::BitmapImage> bitmapImage = Noesis::MakePtr<Noesis::BitmapImage>();
    bitmapImage->SetUriSource(Noesis::Uri("GUI/Cards/Note_Card.png"));
    backgroundImage->SetSource(bitmapImage);
    backgroundImage->SetStretch(Noesis::Stretch_Fill);
    Noesis::Grid::SetRowSpan(backgroundImage, 2);
    cardContainer->GetChildren()->Add(backgroundImage);

    // Speaker label (top, centered, smaller font for smaller card)
    Noesis::Ptr<Noesis::TextBlock> speakerLabel = Noesis::MakePtr<Noesis::TextBlock>();
    speakerLabel->SetText(speaker.c_str());
    speakerLabel->SetFontSize(14.0f);
    speakerLabel->SetFontWeight(Noesis::FontWeight_Bold);
    speakerLabel->SetTextWrapping(Noesis::TextWrapping_Wrap);
    speakerLabel->SetTextAlignment(Noesis::TextAlignment_Center);
    speakerLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Center);
    speakerLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    speakerLabel->SetMargin(Noesis::Thickness(10, 8, 10, 4));
    speakerLabel->SetEffect(nullptr); // No shadow
    
    // Use Opera Lyrics font
    Noesis::Ptr<Noesis::FontFamily> fontFamily =
        Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth");
    speakerLabel->SetFontFamily(fontFamily);
    
    // Dark color
    Noesis::Ptr<Noesis::SolidColorBrush> textBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(40, 35, 30));
    speakerLabel->SetForeground(textBrush);
    
    Noesis::Grid::SetRow(speakerLabel, 0);
    cardContainer->GetChildren()->Add(speakerLabel);

    // Message text (bottom area, wrapped, smaller font for smaller card)
    Noesis::Ptr<Noesis::TextBlock> messageText = Noesis::MakePtr<Noesis::TextBlock>();
    std::string quotedMessage = "\"" + message + "\"";
    messageText->SetText(quotedMessage.c_str());
    messageText->SetFontSize(10.0f);
    messageText->SetTextWrapping(Noesis::TextWrapping_Wrap);
    messageText->SetTextAlignment(Noesis::TextAlignment_Left);
    messageText->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    messageText->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    messageText->SetMargin(Noesis::Thickness(12, 6, 12, 12));
    messageText->SetEffect(nullptr); // No shadow
    messageText->SetFontFamily(fontFamily);
    messageText->SetForeground(textBrush);
    
    Noesis::Grid::SetRow(messageText, 1);
    cardContainer->GetChildren()->Add(messageText);

    // Add drop shadow to entire card
    Noesis::Ptr<Noesis::DropShadowEffect> shadow = Noesis::MakePtr<Noesis::DropShadowEffect>();
    shadow->SetColor(Noesis::Color(0, 0, 0));
    shadow->SetBlurRadius(10.0f);
    shadow->SetShadowDepth(4.0f);
    shadow->SetOpacity(0.5f);
    cardContainer->SetEffect(shadow);

    // Position card in board space (centered)
    Noesis::Canvas::SetLeft(cardContainer, boardX - 90.0f);
    Noesis::Canvas::SetTop(cardContainer, boardY - 116.0f);

    // Add to caseboard content
    caseboardContent->GetChildren()->Add(cardContainer);

    // Store in testimony cards list
    testimony.container = cardContainer;
    testimony.speakerLabel = speakerLabel;
    testimony.messageText = messageText;
    testimonyCards.push_back(testimony);

    wi::backlog::post("AddTestimonyCard: Testimony card created successfully!\n");
}

int CaseboardMode::HitTestTestimonyCard(float boardX, float boardY) {
    // Check from top to bottom (reverse order for top-most card first)
    for (int i = (int)testimonyCards.size() - 1; i >= 0; i--) {
        TestimonyCard &card = testimonyCards[i];

        float cardLeft = card.boardX - card.width / 2.0f;
        float cardTop = card.boardY - card.height / 2.0f;
        float cardRight = cardLeft + card.width;
        float cardBottom = cardTop + card.height;

        if (boardX >= cardLeft && boardX <= cardRight && boardY >= cardTop &&
            boardY <= cardBottom) {
            return i;
        }
    }
    return -1;
}

void CaseboardMode::StartDraggingTestimonyCard(int index, float boardX, float boardY) {
    if (index < 0 || index >= (int)testimonyCards.size())
        return;

    draggingTestimonyCardIndex = index;
    TestimonyCard &card = testimonyCards[index];

    dragOffsetX = boardX - card.boardX;
    dragOffsetY = boardY - card.boardY;

    wi::backlog::post("Started dragging testimony card\n");
}

void CaseboardMode::UpdateDraggingTestimonyCard(float boardX, float boardY) {
    if (draggingTestimonyCardIndex < 0 || draggingTestimonyCardIndex >= (int)testimonyCards.size())
        return;

    TestimonyCard &card = testimonyCards[draggingTestimonyCardIndex];

    card.boardX = boardX - dragOffsetX;
    card.boardY = boardY - dragOffsetY;

    if (card.container) {
        Noesis::Canvas::SetLeft(card.container, card.boardX - card.width / 2.0f);
        Noesis::Canvas::SetTop(card.container, card.boardY - card.height / 2.0f);
    }
}

void CaseboardMode::StopDraggingTestimonyCard() {
    if (draggingTestimonyCardIndex >= 0) {
        wi::backlog::post("Stopped dragging testimony card\n");
    }
    draggingTestimonyCardIndex = -1;
}

void CaseboardMode::AddCaseFile(const std::string &photoFilename, const std::string &npcName) {
    if (!caseboardContent) {
        wi::backlog::post("AddCaseFile: caseboardContent is null!\n");
        return;
    }

    wi::backlog::post("AddCaseFile: Creating case-file...\n");

    // Calculate position in board space (offset from photo cards)
    float boardX = -500.0f + (caseFiles.size() % 5) * 210.0f;
    float boardY = -400.0f + (caseFiles.size() / 5) * 280.0f;

    // Case-file dimensions (same width as case cards: 180px)
    const float fileWidth = 180.0f;
    const float fileHeight = 232.0f; // Maintain similar aspect ratio
    const float tabWidth = 20.0f;    // Width of the tab on the right edge
    const float tabHeight = 140.0f;  // 60% of file height

    // Create the case-file container
    Noesis::Ptr<Noesis::Canvas> fileContainer = *new Noesis::Canvas();
    fileContainer->SetWidth(fileWidth + tabWidth);
    fileContainer->SetHeight(fileHeight);

    // Layer 1a: Yellow background for cover (with rounded right edge)
    Noesis::Ptr<Noesis::Border> coverBackground = *new Noesis::Border();
    coverBackground->SetWidth(fileWidth);
    coverBackground->SetHeight(fileHeight);
    coverBackground->SetBackground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(255, 235, 120))); // Yellow
    coverBackground->SetBorderBrush(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(180, 160, 80)));
    coverBackground->SetBorderThickness(Noesis::Thickness(2.0f));
    coverBackground->SetCornerRadius(Noesis::CornerRadius(0, 8.0f, 8.0f, 0)); // Rounded right edge
    Noesis::Canvas::SetLeft(coverBackground, 0);
    Noesis::Canvas::SetTop(coverBackground, 0);
    fileContainer->GetChildren()->Add(coverBackground);

    // Layer 1b: Notepad background for content pages (initially hidden)
    Noesis::Ptr<Noesis::Image> pageBackground = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> pageBgBitmap = *new Noesis::BitmapImage();
    pageBgBitmap->SetUriSource(Noesis::Uri("GUI/Cards/Note_Card.png"));
    pageBackground->SetSource(pageBgBitmap);
    pageBackground->SetStretch(Noesis::Stretch_Fill);
    pageBackground->SetWidth(fileWidth);
    pageBackground->SetHeight(fileHeight);
    pageBackground->SetVisibility(Noesis::Visibility_Collapsed); // Hidden on cover
    Noesis::Canvas::SetLeft(pageBackground, 0);
    Noesis::Canvas::SetTop(pageBackground, 0);
    fileContainer->GetChildren()->Add(pageBackground);

    // Layer 2a: Right tab (for forward navigation)
    Noesis::Ptr<Noesis::Border> rightTab = *new Noesis::Border();
    rightTab->SetWidth(tabWidth);
    rightTab->SetHeight(tabHeight);
    rightTab->SetBackground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(255, 220, 100))); // Slightly darker yellow
    rightTab->SetBorderBrush(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(180, 160, 80)));
    rightTab->SetBorderThickness(Noesis::Thickness(2.0f, 2.0f, 0, 2.0f));
    rightTab->SetCornerRadius(Noesis::CornerRadius(0, 6.0f, 6.0f, 0)); // Rounded right edge
    Noesis::Canvas::SetLeft(rightTab, fileWidth - 0.5f);                // Overlap slightly
    Noesis::Canvas::SetTop(rightTab, fileHeight * 0.2f);                // Start 20% down
    fileContainer->GetChildren()->Add(rightTab);

    // Layer 2b: Triangle on right tab pointing right (centered)
    Noesis::Ptr<Noesis::TextBlock> rightTabArrow = *new Noesis::TextBlock();
    rightTabArrow->SetText("▶"); // Right-pointing triangle
    rightTabArrow->SetFontSize(14.0f);
    rightTabArrow->SetForeground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0, 0, 0))); // Black
    rightTabArrow->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    rightTabArrow->SetVerticalAlignment(Noesis::VerticalAlignment_Center);
    rightTabArrow->SetWidth(tabWidth);
    rightTabArrow->SetHeight(tabHeight);
    Noesis::Canvas::SetLeft(rightTabArrow, fileWidth - 0.5f);
    Noesis::Canvas::SetTop(rightTabArrow, fileHeight * 0.2f);
    fileContainer->GetChildren()->Add(rightTabArrow);

    // Layer 2c: Left tab (for backward navigation, initially hidden)
    Noesis::Ptr<Noesis::Border> leftTab = *new Noesis::Border();
    leftTab->SetWidth(tabWidth);
    leftTab->SetHeight(tabHeight);
    leftTab->SetBackground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(255, 220, 100))); // Slightly darker yellow
    leftTab->SetBorderBrush(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(180, 160, 80)));
    leftTab->SetBorderThickness(Noesis::Thickness(0, 2.0f, 2.0f, 2.0f));
    leftTab->SetCornerRadius(Noesis::CornerRadius(6.0f, 0, 0, 6.0f)); // Rounded left edge
    Noesis::Canvas::SetLeft(leftTab, -tabWidth + 0.5f);                // Overlap slightly on left
    Noesis::Canvas::SetTop(leftTab, fileHeight * 0.2f);                // Start 20% down
    leftTab->SetVisibility(Noesis::Visibility_Collapsed);              // Hidden initially
    fileContainer->GetChildren()->Add(leftTab);

    // Layer 2d: Triangle on left tab pointing left (centered)
    Noesis::Ptr<Noesis::TextBlock> leftTabArrow = *new Noesis::TextBlock();
    leftTabArrow->SetText("◀"); // Left-pointing triangle
    leftTabArrow->SetFontSize(14.0f);
    leftTabArrow->SetForeground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0, 0, 0))); // Black
    leftTabArrow->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    leftTabArrow->SetVerticalAlignment(Noesis::VerticalAlignment_Center);
    leftTabArrow->SetWidth(tabWidth);
    leftTabArrow->SetHeight(tabHeight);
    Noesis::Canvas::SetLeft(leftTabArrow, -tabWidth + 0.5f);
    Noesis::Canvas::SetTop(leftTabArrow, fileHeight * 0.2f);
    leftTabArrow->SetVisibility(Noesis::Visibility_Collapsed); // Hidden initially
    fileContainer->GetChildren()->Add(leftTabArrow);

    // Layer 3: Photo image (smaller, centered on yellow background)
    const float photoMarginH = 10.0f;     // Horizontal margin
    const float photoMarginBottom = 10.0f; // Bottom margin
    const float photoAreaTop = 35.0f;      // Leave space for NPC name
    const float photoAreaHeight = fileHeight - photoAreaTop - photoMarginBottom;

    Noesis::Ptr<Noesis::Grid> photoImageContainer = *new Noesis::Grid();
    photoImageContainer->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    photoImageContainer->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoImageContainer->SetMargin(
        Noesis::Thickness(photoMarginH, photoAreaTop, photoMarginH + tabWidth, photoMarginBottom));

    Noesis::Ptr<Noesis::Image> photoImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> photoBitmap = *new Noesis::BitmapImage();
    photoBitmap->SetUriSource(Noesis::Uri(photoFilename.c_str()));
    photoImage->SetSource(photoBitmap);
    photoImage->SetStretch(Noesis::Stretch_Uniform);
    photoImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    photoImage->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoImageContainer->GetChildren()->Add(photoImage);
    Noesis::Canvas::SetLeft(photoImageContainer, 0);
    Noesis::Canvas::SetTop(photoImageContainer, 0);
    fileContainer->GetChildren()->Add(photoImageContainer);

    // Layer 4: NPC Name label at the top (on cover page)
    Noesis::Ptr<Noesis::TextBlock> npcLabel = *new Noesis::TextBlock();
    npcLabel->SetText(npcName.c_str());
    npcLabel->SetFontSize(16.0f);
    npcLabel->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth"));
    npcLabel->SetForeground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(10, 15, 25))); // Very dark blue, nearly black
    npcLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    npcLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    npcLabel->SetTextAlignment(Noesis::TextAlignment_Center);
    npcLabel->SetEffect(nullptr); // No shadow
    npcLabel->SetFontWeight(Noesis::FontWeight_Bold);
    Noesis::Canvas::SetLeft(npcLabel, photoMarginH);
    Noesis::Canvas::SetTop(npcLabel, 8.0f);
    Noesis::Canvas::SetRight(npcLabel, photoMarginH + tabWidth);
    fileContainer->GetChildren()->Add(npcLabel);

    // Layer 5: Page content panel (for displaying fields on pages 1+)
    Noesis::Ptr<Noesis::StackPanel> pageContentPanel = *new Noesis::StackPanel();
    pageContentPanel->SetOrientation(Noesis::Orientation_Vertical);
    pageContentPanel->SetVisibility(Noesis::Visibility_Collapsed); // Hidden on cover
    Noesis::Canvas::SetLeft(pageContentPanel, photoMarginH);
    Noesis::Canvas::SetTop(pageContentPanel, photoAreaTop);
    Noesis::Canvas::SetRight(pageContentPanel, photoMarginH + tabWidth);
    fileContainer->GetChildren()->Add(pageContentPanel);

    // Add drop shadow effect to entire file
    Noesis::Ptr<Noesis::DropShadowEffect> shadowEffect = *new Noesis::DropShadowEffect();
    shadowEffect->SetColor(Noesis::Color(0, 0, 0));
    shadowEffect->SetOpacity(0.6f);
    shadowEffect->SetBlurRadius(12.0f);
    shadowEffect->SetShadowDepth(5.0f);
    fileContainer->SetEffect(shadowEffect);

    // Position on canvas (center the file at boardX, boardY)
    Noesis::Canvas::SetLeft(fileContainer, boardX - (fileWidth + tabWidth) / 2.0f);
    Noesis::Canvas::SetTop(fileContainer, boardY - fileHeight / 2.0f);

    // Add to caseboard
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(fileContainer);
        char buf[128];
        sprintf_s(buf, "AddCaseFile: Added to caseboard, now has %d children\n", children->Count());
        wi::backlog::post(buf);
    } else {
        wi::backlog::post("AddCaseFile: Could not get children collection!\n");
    }

    // Store reference
    CaseFile caseFile;
    caseFile.container = fileContainer;
    caseFile.coverBackground = coverBackground;
    caseFile.pageBackground = pageBackground;
    caseFile.photoImageContainer = photoImageContainer;
    caseFile.coverPhoto = photoImage;
    caseFile.npcNameLabel = npcLabel;
    caseFile.pageContentPanel = pageContentPanel;
    caseFile.leftTab = leftTab;
    caseFile.leftTabArrow = leftTabArrow;
    caseFile.boardX = boardX;
    caseFile.boardY = boardY;
    caseFile.npcName = npcName;
    caseFile.photoFilename = photoFilename;
    caseFile.width = fileWidth + tabWidth;
    caseFile.height = fileHeight;
    caseFile.currentPage = 0;
    caseFile.isOpen = false;

    // Populate with dynamic pages
    PopulateCaseFilePages(caseFile);

    caseFiles.push_back(caseFile);

    char buf[256];
    sprintf_s(buf, "Created case-file for NPC '%s' at board position (%.0f, %.0f)\n",
              npcName.c_str(), boardX, boardY);
    wi::backlog::post(buf);
}

int CaseboardMode::HitTestCaseFile(float boardX, float boardY) {
    // Check from top to bottom (reverse order for top-most file first)
    for (int i = (int)caseFiles.size() - 1; i >= 0; i--) {
        CaseFile &file = caseFiles[i];

        float fileLeft = file.boardX - file.width / 2.0f;
        float fileTop = file.boardY - file.height / 2.0f;
        float fileRight = fileLeft + file.width;
        float fileBottom = fileTop + file.height;

        if (boardX >= fileLeft && boardX <= fileRight && boardY >= fileTop &&
            boardY <= fileBottom) {
            return i;
        }
    }
    return -1;
}

void CaseboardMode::HandleCaseFileClick(int caseFileIndex, float localX, float localY) {
    if (caseFileIndex < 0 || caseFileIndex >= (int)caseFiles.size())
        return;

    CaseFile &file = caseFiles[caseFileIndex];

    // Clicked on right tab - navigate to next page
    file.currentPage++;
    if (file.currentPage > (int)file.pages.size()) {
        file.currentPage = 0; // Wrap back to cover
    }

    char buf[128];
    sprintf_s(buf, "Case-file '%s' page changed to: %d\n", file.npcName.c_str(), file.currentPage);
    wi::backlog::post(buf);

    // Update the visual display
    UpdateCaseFilePageDisplay(file);
}

void CaseboardMode::HandleCaseFileLeftTabClick(int caseFileIndex) {
    if (caseFileIndex < 0 || caseFileIndex >= (int)caseFiles.size())
        return;

    CaseFile &file = caseFiles[caseFileIndex];

    // Go back one page
    file.currentPage--;
    if (file.currentPage < 0) {
        file.currentPage = 0; // Stay on cover
    }

    char buf[128];
    sprintf_s(buf, "Case-file '%s' page changed to: %d (back)\n", file.npcName.c_str(),
              file.currentPage);
    wi::backlog::post(buf);

    // Update the visual display
    UpdateCaseFilePageDisplay(file);
}

void CaseboardMode::PopulateCaseFilePages(CaseFile &file) {
    // Page 1: Physical description
    CaseFilePage page1;
    page1.fields.push_back({"Name", "Unknown"});
    page1.fields.push_back({"Date of Birth", "Oct 2, 1865"});
    page1.fields.push_back({"Height", "6'1\""});
    page1.fields.push_back({"Weight", "130 lbs"});
    file.pages.push_back(page1);

    // Page 2: Additional details
    CaseFilePage page2;
    page2.fields.push_back({"Eye color", "Blue"});
    page2.fields.push_back({"Hair color", "Brown"});
    page2.fields.push_back({"Occupation", "Unknown"});
    page2.fields.push_back({"Nationality", "Unknown"});
    page2.fields.push_back({"Accent", "English"});
    file.pages.push_back(page2);
}

void CaseboardMode::UpdateCaseFilePageDisplay(CaseFile &file) {
    if (file.currentPage == 0) {
        // Show cover: yellow background, photo + name visible, page content hidden
        if (file.coverBackground) {
            file.coverBackground->SetVisibility(Noesis::Visibility_Visible);
        }
        if (file.pageBackground) {
            file.pageBackground->SetVisibility(Noesis::Visibility_Collapsed);
        }
        if (file.photoImageContainer) {
            file.photoImageContainer->SetVisibility(Noesis::Visibility_Visible);
        }
        if (file.npcNameLabel) {
            file.npcNameLabel->SetVisibility(Noesis::Visibility_Visible);
        }
        if (file.pageContentPanel) {
            file.pageContentPanel->SetVisibility(Noesis::Visibility_Collapsed);
        }
        // Hide left tab on cover
        if (file.leftTab) {
            file.leftTab->SetVisibility(Noesis::Visibility_Collapsed);
        }
        if (file.leftTabArrow) {
            file.leftTabArrow->SetVisibility(Noesis::Visibility_Collapsed);
        }
    } else {
        // Show page content: notepad background, hide cover elements, show fields
        if (file.coverBackground) {
            file.coverBackground->SetVisibility(Noesis::Visibility_Collapsed);
        }
        if (file.pageBackground) {
            file.pageBackground->SetVisibility(Noesis::Visibility_Visible);
        }
        if (file.photoImageContainer) {
            file.photoImageContainer->SetVisibility(Noesis::Visibility_Collapsed);
        }
        if (file.npcNameLabel) {
            file.npcNameLabel->SetVisibility(Noesis::Visibility_Collapsed);
        }
        if (file.pageContentPanel) {
            file.pageContentPanel->SetVisibility(Noesis::Visibility_Visible);

            // Clear existing content
            file.pageContentPanel->GetChildren()->Clear();

            // Get the page (currentPage is 1-based for pages)
            int pageIndex = file.currentPage - 1;
            if (pageIndex >= 0 && pageIndex < (int)file.pages.size()) {
                const CaseFilePage &page = file.pages[pageIndex];

                // Add each field (no page title)
                for (const auto &field : page.fields) {
                    // Field container
                    Noesis::Ptr<Noesis::StackPanel> fieldPanel = *new Noesis::StackPanel();
                    fieldPanel->SetOrientation(Noesis::Orientation_Horizontal);
                    fieldPanel->SetMargin(Noesis::Thickness(0, 2, 0, 2));

                    // Label
                    Noesis::Ptr<Noesis::TextBlock> labelText = *new Noesis::TextBlock();
                    labelText->SetText((field.label + ": ").c_str());
                    labelText->SetFontSize(10.0f);
                    labelText->SetFontFamily(
                        Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth"));
                    labelText->SetForeground(Noesis::MakePtr<Noesis::SolidColorBrush>(
                        Noesis::Color(10, 15, 25))); // Very dark blue, nearly black
                    labelText->SetFontWeight(Noesis::FontWeight_Bold);
                    labelText->SetEffect(nullptr); // No shadow
                    fieldPanel->GetChildren()->Add(labelText);

                    // Value
                    Noesis::Ptr<Noesis::TextBlock> valueText = *new Noesis::TextBlock();
                    valueText->SetText(field.value.c_str());
                    valueText->SetFontSize(10.0f);
                    valueText->SetFontFamily(
                        Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth"));
                    valueText->SetForeground(Noesis::MakePtr<Noesis::SolidColorBrush>(
                        Noesis::Color(10, 15, 25))); // Very dark blue, nearly black
                    valueText->SetEffect(nullptr); // No shadow
                    fieldPanel->GetChildren()->Add(valueText);

                    file.pageContentPanel->GetChildren()->Add(fieldPanel);
                }
            }
        }
        // Show left tab when not on cover
        if (file.leftTab) {
            file.leftTab->SetVisibility(Noesis::Visibility_Visible);
        }
        if (file.leftTabArrow) {
            file.leftTabArrow->SetVisibility(Noesis::Visibility_Visible);
        }
    }
}

void CaseboardMode::StartDraggingCaseFile(int index, float boardX, float boardY) {
    if (index < 0 || index >= (int)caseFiles.size())
        return;

    draggingCaseFileIndex = index;
    CaseFile &file = caseFiles[index];

    dragOffsetX = boardX - file.boardX;
    dragOffsetY = boardY - file.boardY;

    wi::backlog::post("Started dragging case-file\n");
}

void CaseboardMode::UpdateDraggingCaseFile(float boardX, float boardY) {
    if (draggingCaseFileIndex < 0 || draggingCaseFileIndex >= (int)caseFiles.size())
        return;

    CaseFile &file = caseFiles[draggingCaseFileIndex];

    file.boardX = boardX - dragOffsetX;
    file.boardY = boardY - dragOffsetY;

    if (file.container) {
        Noesis::Canvas::SetLeft(file.container, file.boardX - file.width / 2.0f);
        Noesis::Canvas::SetTop(file.container, file.boardY - file.height / 2.0f);
    }
}

void CaseboardMode::StopDraggingCaseFile() {
    if (draggingCaseFileIndex >= 0) {
        wi::backlog::post("Stopped dragging case-file\n");
    }
    draggingCaseFileIndex = -1;
}

void CaseboardMode::AddPhotoCard(const std::string &photoFilename) {
    if (!caseboardContent) {
        wi::backlog::post("AddPhotoCard: caseboardContent is null!\n");
        return;
    }

    wi::backlog::post("AddPhotoCard: Creating photo card...\n");

    // Calculate position in board space
    float boardX = 500.0f + (photoCards.size() % 5) * 250.0f;
    float boardY = -400.0f + (photoCards.size() / 5) * 350.0f;

    // Physical evidence card dimensions - match black cards width (180px)
    // Maintain original aspect ratio: 256x320 (4:5 ratio)
    // New: 180x225
    const float cardWidth = 180.0f;
    const float cardHeight = 225.0f;

    // Layout constants (scaled proportionally to new card size):
    // - Label height: 28px (reduced from 32px)
    // - Image margin: 12px (reduced from 16px)
    const float imageMargin = 12.0f;
    const float labelHeight = 28.0f;
    const float imageAreaTop = labelHeight + imageMargin;
    const float imageAreaBottom = imageMargin;

    // Create the photo card container (Grid with fixed dimensions)
    Noesis::Ptr<Noesis::Grid> photoContainer = *new Noesis::Grid();
    photoContainer->SetWidth(cardWidth);
    photoContainer->SetHeight(cardHeight);

    // Layer 1: Background - Physical evidence card bitmap
    Noesis::Ptr<Noesis::Image> cardBackground = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> cardBgBitmap = *new Noesis::BitmapImage();
    // Use Note_Card.png as the base (it's a generic card design)
    cardBgBitmap->SetUriSource(Noesis::Uri("GUI/Cards/Evidence_CARD_Base.png"));
    cardBackground->SetSource(cardBgBitmap);
    cardBackground->SetStretch(Noesis::Stretch_Fill);
    cardBackground->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    cardBackground->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoContainer->GetChildren()->Add(cardBackground);

    // Layer 2: Photo image (positioned with margins matching Unreal Card.h layout)
    // The image occupies the space: [imageMargin, imageAreaTop] to [cardWidth-imageMargin,
    // cardHeight-imageAreaBottom]
    Noesis::Ptr<Noesis::Grid> photoImageContainer = *new Noesis::Grid();
    photoImageContainer->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    photoImageContainer->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoImageContainer->SetMargin(
        Noesis::Thickness(imageMargin, imageAreaTop, imageMargin, imageAreaBottom));

    Noesis::Ptr<Noesis::Image> photoImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> photoBitmap = *new Noesis::BitmapImage();
    photoBitmap->SetUriSource(Noesis::Uri(photoFilename.c_str()));
    photoImage->SetSource(photoBitmap);
    photoImage->SetStretch(Noesis::Stretch_Uniform);
    photoImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Stretch);
    photoImage->SetVerticalAlignment(Noesis::VerticalAlignment_Stretch);
    photoImageContainer->GetChildren()->Add(photoImage);
    photoContainer->GetChildren()->Add(photoImageContainer);

    // Layer 3: Label (centered at the top, within labelHeight area)
    Noesis::Ptr<Noesis::TextBlock> photoLabel = *new Noesis::TextBlock();
    std::string labelText = "Photo";
    photoLabel->SetText(labelText.c_str());
    photoLabel->SetFontSize(16.0f);
    photoLabel->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth"));
    photoLabel->SetForeground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(0x2B, 0x3D, 0x5C))); // Dark ink color
    photoLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    photoLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    photoLabel->SetMargin(Noesis::Thickness(imageMargin, 6.0f, imageMargin, 0));
    photoLabel->SetEffect(nullptr);
    photoLabel->SetFontWeight(Noesis::FontWeight_Normal);
    photoLabel->SetTextAlignment(Noesis::TextAlignment_Center);
    photoContainer->GetChildren()->Add(photoLabel);

    // Add drop shadow effect to entire card (matching Unreal shadow rendering)
    Noesis::Ptr<Noesis::DropShadowEffect> shadowEffect = *new Noesis::DropShadowEffect();
    shadowEffect->SetColor(Noesis::Color(0, 0, 0));
    shadowEffect->SetOpacity(0.5f);
    shadowEffect->SetBlurRadius(12.0f);
    shadowEffect->SetShadowDepth(6.0f);
    photoContainer->SetEffect(shadowEffect);

    // Position on canvas (center the card at boardX, boardY)
    Noesis::Canvas::SetLeft(photoContainer, boardX - cardWidth / 2.0f);
    Noesis::Canvas::SetTop(photoContainer, boardY - cardHeight / 2.0f);

    // Add to caseboard
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(photoContainer);
        char buf[128];
        sprintf_s(buf, "AddPhotoCard: Added to caseboard, now has %d children\n",
                  children->Count());
        wi::backlog::post(buf);
    } else {
        wi::backlog::post("AddPhotoCard: Could not get children collection!\n");
    }

    // Store reference
    PhotoCard card;
    card.container = photoContainer;
    card.photoImage = photoImage;
    card.label = photoLabel;
    card.boardX = boardX;
    card.boardY = boardY;
    card.labelText = labelText;
    card.width = cardWidth;
    card.height = cardHeight;
    photoCards.push_back(card);
}
