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
#include <NsGui/SolidColorBrush.h>
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
    caseboardZoom = std::clamp(caseboardZoom + zoomDelta, 0.2f, 4.0f);

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
                continue;

            float cardLeft = card.boardX - 90.0f + 15.0f;
            float cardTop = card.boardY - 110.0f + 40.0f;
            float cardRight = card.boardX - 90.0f + 180.0f - 15.0f;
            float cardBottom = card.boardY - 110.0f + 220.0f - 20.0f;

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

    // Check if clicking on a case-file
    int caseFileIndex = HitTestCaseFile(boardClickX, boardClickY);
    if (caseFileIndex >= 0) {
        CaseFile &file = caseFiles[caseFileIndex];
        float fileLeft = file.boardX - file.width / 2.0f;
        float fileTop = file.boardY - file.height / 2.0f;
        float localX = boardClickX - fileLeft;
        float localY = boardClickY - fileTop;

        // Check if clicking on the tab area (right side)
        float tabStartX = file.width - 20.5f; // Tab starts at this X position (tabWidth + margin)

        if (localX >= tabStartX) {
            // Clicked on tab - navigate to next page
            HandleCaseFileClick(caseFileIndex, localX, localY);
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

    // Create the note card container
    Noesis::Ptr<Noesis::Grid> noteContainer = *new Noesis::Grid();
    noteContainer->SetWidth(180.0f);
    noteContainer->SetHeight(220.0f);

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
    textBox->SetFontSize(16.0f);
    textBox->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Old Man Eloquent"));

    Noesis::Ptr<Noesis::SolidColorBrush> inkBrush =
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C));
    textBox->SetForeground(inkBrush);
    textBox->SetBackground(nullptr);
    textBox->SetBorderThickness(Noesis::Thickness(0.0f));
    textBox->SetCaretBrush(inkBrush);
    noteContainer->GetChildren()->Add(textBox);

    // Position on canvas
    Noesis::Canvas::SetLeft(noteContainer, boardX - 90.0f);
    Noesis::Canvas::SetTop(noteContainer, boardY - 110.0f);

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
    textLabel->SetFontSize(16.0f);
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
        float cardTop = card.boardY - 110.0f;
        float cardRight = cardLeft + 180.0f;
        float cardBottom = cardTop + 220.0f;

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

    // Layer 1: Yellow background with rounded right edge
    Noesis::Ptr<Noesis::Border> fileBackground = *new Noesis::Border();
    fileBackground->SetWidth(fileWidth);
    fileBackground->SetHeight(fileHeight);
    fileBackground->SetBackground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(255, 235, 120))); // Yellow
    fileBackground->SetBorderBrush(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(180, 160, 80)));
    fileBackground->SetBorderThickness(Noesis::Thickness(2.0f));
    fileBackground->SetCornerRadius(Noesis::CornerRadius(0, 8.0f, 8.0f, 0)); // Rounded right edge
    Noesis::Canvas::SetLeft(fileBackground, 0);
    Noesis::Canvas::SetTop(fileBackground, 0);
    fileContainer->GetChildren()->Add(fileBackground);

    // Layer 2: Tab (rectangle with triangle)
    Noesis::Ptr<Noesis::Border> fileTab = *new Noesis::Border();
    fileTab->SetWidth(tabWidth);
    fileTab->SetHeight(tabHeight);
    fileTab->SetBackground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(255, 220, 100))); // Slightly darker yellow
    fileTab->SetBorderBrush(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(180, 160, 80)));
    fileTab->SetBorderThickness(Noesis::Thickness(2.0f, 2.0f, 0, 2.0f));
    fileTab->SetCornerRadius(Noesis::CornerRadius(0, 6.0f, 6.0f, 0)); // Rounded right edge
    Noesis::Canvas::SetLeft(fileTab, fileWidth - 0.5f);               // Overlap slightly
    Noesis::Canvas::SetTop(fileTab, fileHeight * 0.2f);               // Start 20% down
    fileContainer->GetChildren()->Add(fileTab);

    // Layer 2b: Triangle on tab pointing right (simple arrow using text)
    Noesis::Ptr<Noesis::TextBlock> tabArrow = *new Noesis::TextBlock();
    tabArrow->SetText("▶"); // Right-pointing triangle
    tabArrow->SetFontSize(14.0f);
    tabArrow->SetForeground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C))); // Dark ink
    tabArrow->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    tabArrow->SetVerticalAlignment(Noesis::VerticalAlignment_Center);
    Noesis::Canvas::SetLeft(tabArrow, fileWidth - 0.5f + tabWidth / 2.0f - 7.0f);
    Noesis::Canvas::SetTop(tabArrow, fileHeight * 0.2f + tabHeight / 2.0f - 7.0f);
    fileContainer->GetChildren()->Add(tabArrow);

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

    // Layer 4: NPC Name label at the top
    Noesis::Ptr<Noesis::TextBlock> npcLabel = *new Noesis::TextBlock();
    npcLabel->SetText(npcName.c_str());
    npcLabel->SetFontSize(16.0f);
    npcLabel->SetFontFamily(Noesis::MakePtr<Noesis::FontFamily>("Fonts/#Opera-Lyrics-Smooth"));
    npcLabel->SetForeground(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0x2B, 0x3D, 0x5C))); // Dark ink
    npcLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    npcLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    npcLabel->SetTextAlignment(Noesis::TextAlignment_Center);
    npcLabel->SetEffect(nullptr);
    npcLabel->SetFontWeight(Noesis::FontWeight_Bold);
    Noesis::Canvas::SetLeft(npcLabel, photoMarginH);
    Noesis::Canvas::SetTop(npcLabel, 8.0f);
    Noesis::Canvas::SetRight(npcLabel, photoMarginH + tabWidth);
    fileContainer->GetChildren()->Add(npcLabel);

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
    caseFile.coverPhoto = photoImage;
    caseFile.npcNameLabel = npcLabel;
    caseFile.boardX = boardX;
    caseFile.boardY = boardY;
    caseFile.npcName = npcName;
    caseFile.photoFilename = photoFilename;
    caseFile.width = fileWidth + tabWidth;
    caseFile.height = fileHeight;
    caseFile.currentPage = 0;
    caseFile.isOpen = false;

    // Initialize with some placeholder pages
    caseFile.pages.push_back("Page 1: Investigation notes...");
    caseFile.pages.push_back("Page 2: Additional information...");
    caseFile.pages.push_back("Page 3: Conclusion...");

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

    // Clicked on tab - navigate to next page
    file.currentPage++;
    if (file.currentPage > (int)file.pages.size()) {
        file.currentPage = 0; // Wrap back to cover
    }

    char buf[128];
    sprintf_s(buf, "Case-file '%s' page changed to: %d\n", file.npcName.c_str(), file.currentPage);
    wi::backlog::post(buf);

    // Update visual feedback (optional - could show page number or different content)
    if (file.currentPage == 0) {
        wi::backlog::post("  Showing cover\n");
    } else if (file.currentPage <= (int)file.pages.size()) {
        char pageBuf[256];
        sprintf_s(pageBuf, "  Showing page %d: %s\n", file.currentPage,
                  file.pages[file.currentPage - 1].c_str());
        wi::backlog::post(pageBuf);
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

    // Physical evidence card dimensions from original Unreal (Card.h), scaled up 2x:
    // Original: 128x160 -> Scaled: 256x320
    const float cardWidth = 256.0f;
    const float cardHeight = 320.0f;

    // Layout constants from Card.h (scaled 2x):
    // - Label height: 16px -> 32px
    // - Image margin: 8px -> 16px (on left, right, and bottom)
    // - Image area: 112x136px -> 224x272px
    const float imageMargin = 16.0f;
    const float labelHeight = 32.0f;
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
    photoLabel->SetFontSize(20.0f);
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
