#include "CaseboardMode.h"

#include <NsCore/DynamicCast.h>
#include <NsDrawing/Color.h>
#include <NsDrawing/CornerRadius.h>
#include <NsDrawing/Thickness.h>
#include <NsGui/BitmapImage.h>
#include <NsGui/Border.h>
#include <NsGui/BoxedFreezableCollection.h>
#include <NsGui/Canvas.h>
#include <NsGui/DropShadowEffect.h>
#include <NsGui/Enums.h>
#include <NsGui/FontFamily.h>
#include <NsGui/FontProperties.h>
#include <NsGui/FreezableCollection.h>
#include <NsGui/Grid.h>
#include <NsGui/Image.h>
#include <NsGui/Line.h>
#include <NsGui/Path.h>
#include <NsGui/PathFigure.h>
#include <NsGui/PathGeometry.h>
#include <NsGui/PolyLineSegment.h>
#include <NsGui/Rectangle.h>
#include <NsGui/RotateTransform.h>
#include <NsGui/ScaleTransform.h>
#include <NsGui/Shape.h>

// Undefine Windows macro that conflicts with Noesis::Rectangle
#ifdef Rectangle
#undef Rectangle
#endif
#include <NsGui/RowDefinition.h>
#include <NsGui/SolidColorBrush.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/TextProperties.h>
#include <NsGui/TransformGroup.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/Uri.h>

#include <algorithm>
#include <cmath>

// Forward declare CaseboardMode for helper functions
class CaseboardMode;

// Helper function to update pin visual based on hover state
static void UpdatePinColor(Noesis::Image *pinImage, bool hovering) {
    if (!pinImage)
        return;

    // Ensure transform origin is centered so scaling happens from center
    pinImage->SetRenderTransformOrigin(Noesis::Point(0.5f, 0.5f));

    if (hovering) {
        // Scale up and brighten when hovering (scale from center)
        pinImage->SetOpacity(1.0f);
        Noesis::Ptr<Noesis::ScaleTransform> scale = Noesis::MakePtr<Noesis::ScaleTransform>();
        scale->SetScaleX(32.0f / 24.0f); // Scale from 24 to 32
        scale->SetScaleY(32.0f / 24.0f);
        pinImage->SetRenderTransform(scale);
    } else {
        // Normal size (no transform)
        pinImage->SetOpacity(0.8f);
        pinImage->SetRenderTransform(nullptr);
    }
}

// Helper function to get card position and size by type/index
static bool GetCardGeometry(const CaseboardMode *mode, int cardType, int cardIndex, float &outX,
                            float &outY, float &outWidth, float &outHeight);

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

        // Store reference to the background Rectangle (first child from XAML)
        background = Noesis::Ptr(caseboardContent->GetChildren()->Get(0));
        caseCardsContent = Noesis::Ptr(caseboardContent->GetChildren()->Get(1));

        // Initialize case cards from the StackPanel
        if (caseCardsContent) {
            Noesis::StackPanel *stackPanel =
                Noesis::DynamicCast<Noesis::StackPanel *>(caseCardsContent.GetPtr());
            if (stackPanel) {
                // Get StackPanel position (from XAML: Canvas.Left="-630" Canvas.Top="-125")
                float stackPanelLeft = -630.0f;
                float stackPanelTop = -125.0f;

                // Card dimensions and spacing
                const float cardWidth = 180.0f;
                const float cardHeight = 232.0f;
                const float cardMargin = 15.0f;                          // Margin on each Grid
                const float cardSpacing = cardWidth + cardMargin * 2.0f; // Total spacing per card

                // Iterate through Grid children (each Grid contains one case card)
                Noesis::UIElementCollection *children = stackPanel->GetChildren();
                if (children) {
                    for (int i = 0; i < children->Count(); i++) {
                        Noesis::Grid *cardGrid =
                            Noesis::DynamicCast<Noesis::Grid *>(children->Get(i));
                        if (cardGrid) {
                            CaseCard caseCard;
                            caseCard.container = Noesis::Ptr<Noesis::Grid>(cardGrid);

                            // Find the card image (first child) and pin image (second child)
                            Noesis::UIElementCollection *gridChildren = cardGrid->GetChildren();
                            if (gridChildren && gridChildren->Count() >= 2) {
                                Noesis::Image *cardImg =
                                    Noesis::DynamicCast<Noesis::Image *>(gridChildren->Get(0));
                                caseCard.cardImage = Noesis::Ptr<Noesis::Image>(cardImg);

                                // Get actual dimensions from the rendered image
                                // If not yet rendered, try to get from bitmap source
                                float actualWidth = cardWidth;   // Default to specified width
                                float actualHeight = cardHeight; // Default fallback

                                if (cardImg) {
                                    // Try to get actual rendered size
                                    float renderedWidth = cardImg->GetActualWidth();
                                    float renderedHeight = cardImg->GetActualHeight();

                                    if (renderedWidth > 0.0f && renderedHeight > 0.0f) {
                                        actualWidth = renderedWidth;
                                        actualHeight = renderedHeight;
                                    } else {
                                        // If not rendered yet, try to get from bitmap source
                                        Noesis::ImageSource *source = cardImg->GetSource();
                                        if (source) {
                                            Noesis::BitmapSource *bitmapSource =
                                                Noesis::DynamicCast<Noesis::BitmapSource *>(source);
                                            if (bitmapSource) {
                                                float pixelWidth =
                                                    (float)bitmapSource->GetPixelWidth();
                                                float pixelHeight =
                                                    (float)bitmapSource->GetPixelHeight();
                                                if (pixelWidth > 0.0f && pixelHeight > 0.0f) {
                                                    // Calculate height based on width and aspect
                                                    // ratio
                                                    actualHeight =
                                                        (cardWidth / pixelWidth) * pixelHeight;
                                                }
                                            }
                                        }
                                    }
                                }

                                // Get pin image (second child)
                                Noesis::Image *pinImg =
                                    Noesis::DynamicCast<Noesis::Image *>(gridChildren->Get(1));
                                if (pinImg) {
                                    caseCard.pin.pinImage = Noesis::Ptr<Noesis::Image>(pinImg);
                                    caseCard.pin.pinOffsetY =
                                        actualHeight / 2.0f - 2.0f; // Pin 2 units above bottom
                                }

                                // Store actual dimensions
                                caseCard.width = actualWidth;
                                caseCard.height = actualHeight;
                            } else {
                                // Fallback to default dimensions if children not found
                                caseCard.width = cardWidth;
                                caseCard.height = cardHeight;
                            }

                            // Calculate board position
                            // StackPanel is at (-630, -125), cards are spaced horizontally
                            // Grid Margin="15" offsets each card by cardMargin on all sides
                            caseCard.boardX = stackPanelLeft + cardMargin + caseCard.width / 2.0f +
                                              i * cardSpacing;
                            caseCard.boardY = stackPanelTop + cardMargin + caseCard.height / 2.0f;

                            caseCards.push_back(caseCard);
                        }
                    }

                    char buf[256];
                    sprintf_s(buf, "Initialized %d case cards\n", (int)caseCards.size());
                    wi::backlog::post(buf);
                }
            }
        }
    }

    // Load pin image
    pinBitmapImage = *new Noesis::BitmapImage();
    pinBitmapImage->SetUriSource(Noesis::Uri("GUI/Cards/Pin.png"));
}

void CaseboardMode::Shutdown() {
    caseboardPanel.Reset();
    caseboardContent.Reset();
    caseboardDebugText.Reset();
    caseboardZoomTransform = nullptr;
    caseboardPanTransform = nullptr;
    noteCards.clear();
    photoCards.clear();
    caseCards.clear();
    capturedPhotoTextures.clear();
}

void CaseboardMode::EnterCaseboardMode() {
    if (inCaseboardMode)
        return;

    inCaseboardMode = true;
    caseboardPanning = false;

    // Render any existing connections (don't add test lines that will be cleared)
    // RenderConnections(nullptr);

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

    // Debug: Log click and card counts
    char debugBuf[256];
    sprintf_s(debugBuf, "Click: screen=(%d,%d) board=(%.1f,%.1f) Cards: N=%d P=%d T=%d C=%d\n", x,
              y, boardClickX, boardClickY, (int)noteCards.size(), (int)photoCards.size(),
              (int)testimonyCards.size(), (int)caseFiles.size());
    wi::backlog::post(debugBuf);

    // Check if clicking on a pin to start a connection
    for (int i = 0; i < (int)noteCards.size(); i++) {
        if (HitTestPin(0, i, boardClickX, boardClickY)) {
            StartConnection(0, i);
            return;
        }
    }
    for (int i = 0; i < (int)photoCards.size(); i++) {
        if (HitTestPin(1, i, boardClickX, boardClickY)) {
            StartConnection(1, i);
            return;
        }
    }
    for (int i = 0; i < (int)testimonyCards.size(); i++) {
        if (HitTestPin(2, i, boardClickX, boardClickY)) {
            StartConnection(2, i);
            return;
        }
    }
    for (int i = 0; i < (int)caseFiles.size(); i++) {
        if (HitTestPin(3, i, boardClickX, boardClickY)) {
            StartConnection(3, i);
            return;
        }
    }
    for (int i = 0; i < (int)caseCards.size(); i++) {
        if (HitTestPin(4, i, boardClickX, boardClickY)) {
            StartConnection(4, i);
            return;
        }
    }

    // If we're editing a note and click elsewhere, finalize the edit
    if (editingNoteCardIndex >= 0 && editingNoteCardIndex < (int)noteCards.size()) {
        NoteCard &card = noteCards[editingNoteCardIndex];

        float cardLeft = card.boardX - card.width / 2.0f;
        float cardTop = card.boardY - card.height / 2.0f;
        float cardRight = cardLeft + card.width;
        float cardBottom = cardTop + card.height;

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

            float cardLeft = card.boardX - card.width / 2.0f + 15.0f;
            float cardTop = card.boardY - card.height / 2.0f + 40.0f;
            float cardRight = card.boardX - card.width / 2.0f + card.width - 15.0f;
            float cardBottom = card.boardY - card.height / 2.0f + card.height - 20.0f;

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
        float rightTabStartX =
            file.width - 20.5f; // Tab starts at this X position (tabWidth + margin)
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
    // If we're dragging a connection, end it
    if (draggingConnection) {
        float boardX = (caseboardCurrentMousePos.x - caseboardPanX) / caseboardZoom;
        float boardY = (caseboardCurrentMousePos.y - caseboardPanY) / caseboardZoom;
        EndConnection(boardX, boardY);
        return;
    }

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

    // Handle connection dragging
    if (IsDraggingConnection()) {
        UpdateConnectionDrag(boardX, boardY);
        UpdateCaseboardDebugText();

        // Check if hovering over any pin and change cursor
        bool hoveringPin = false;
        for (int i = 0; i < (int)noteCards.size(); i++) {
            if (noteCards[i].pin.hovering) {
                hoveringPin = true;
                break;
            }
        }
        if (!hoveringPin) {
            for (int i = 0; i < (int)photoCards.size(); i++) {
                if (photoCards[i].pin.hovering) {
                    hoveringPin = true;
                    break;
                }
            }
        }
        if (!hoveringPin) {
            for (int i = 0; i < (int)testimonyCards.size(); i++) {
                if (testimonyCards[i].pin.hovering) {
                    hoveringPin = true;
                    break;
                }
            }
        }
        if (!hoveringPin) {
            for (int i = 0; i < (int)caseFiles.size(); i++) {
                if (caseFiles[i].pin.hovering) {
                    hoveringPin = true;
                    break;
                }
            }
        }
        if (!hoveringPin) {
            for (int i = 0; i < (int)caseCards.size(); i++) {
                if (caseCards[i].pin.hovering) {
                    hoveringPin = true;
                    break;
                }
            }
        }

        // Change cursor during connection drag
        if (windowHandle) {
            if (hoveringPin) {
                SetCursor(LoadCursor(NULL, IDC_CROSS));
            } else {
                SetCursor(LoadCursor(NULL, IDC_SIZEALL)); // Move cursor while dragging
            }
        }
        return;
    }

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
        // Check if hovering over pins (for cursor change and hover state)
        bool hoveringPin = false;
        for (int i = 0; i < (int)noteCards.size(); i++) {
            bool wasHovering = noteCards[i].pin.hovering;
            noteCards[i].pin.hovering = HitTestPin(0, i, boardX, boardY);
            if (noteCards[i].pin.hovering != wasHovering) {
                UpdatePinColor(noteCards[i].pin.pinImage.GetPtr(), noteCards[i].pin.hovering);
            }
            if (noteCards[i].pin.hovering)
                hoveringPin = true;
        }
        for (int i = 0; i < (int)photoCards.size(); i++) {
            bool wasHovering = photoCards[i].pin.hovering;
            photoCards[i].pin.hovering = HitTestPin(1, i, boardX, boardY);
            if (photoCards[i].pin.hovering != wasHovering) {
                UpdatePinColor(photoCards[i].pin.pinImage.GetPtr(), photoCards[i].pin.hovering);
            }
            if (photoCards[i].pin.hovering)
                hoveringPin = true;
        }
        for (int i = 0; i < (int)testimonyCards.size(); i++) {
            bool wasHovering = testimonyCards[i].pin.hovering;
            testimonyCards[i].pin.hovering = HitTestPin(2, i, boardX, boardY);
            if (testimonyCards[i].pin.hovering != wasHovering) {
                UpdatePinColor(testimonyCards[i].pin.pinImage.GetPtr(),
                               testimonyCards[i].pin.hovering);
            }
            if (testimonyCards[i].pin.hovering)
                hoveringPin = true;
        }
        for (int i = 0; i < (int)caseFiles.size(); i++) {
            bool wasHovering = caseFiles[i].pin.hovering;
            caseFiles[i].pin.hovering = HitTestPin(3, i, boardX, boardY);
            if (caseFiles[i].pin.hovering != wasHovering) {
                UpdatePinColor(caseFiles[i].pin.pinImage.GetPtr(), caseFiles[i].pin.hovering);
            }
            if (caseFiles[i].pin.hovering)
                hoveringPin = true;
        }
        for (int i = 0; i < (int)caseCards.size(); i++) {
            bool wasHovering = caseCards[i].pin.hovering;
            caseCards[i].pin.hovering = HitTestPin(4, i, boardX, boardY);
            if (caseCards[i].pin.hovering != wasHovering) {
                UpdatePinColor(caseCards[i].pin.pinImage.GetPtr(), caseCards[i].pin.hovering);
            }
            if (caseCards[i].pin.hovering)
                hoveringPin = true;
        }

        // Change cursor based on what we're hovering over
        if (windowHandle) {
            if (hoveringPin) {
                // Crosshair cursor for pins
                SetCursor(LoadCursor(NULL, IDC_CROSS));
            } else {
                // Check if hovering over a note card drag area, photo card, or case-file
                int hoverCardIndex = HitTestNoteCardDragArea(boardX, boardY);
                int hoverPhotoIndex = HitTestPhotoCard(boardX, boardY);
                int hoverCaseFileIndex = HitTestCaseFile(boardX, boardY);
                if (hoverCardIndex >= 0 || hoverPhotoIndex >= 0 || hoverCaseFileIndex >= 0) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                } else {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            }
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

    // Note card dimensions
    const float noteCardWidth = 180.0f;
    const float noteCardHeight = 232.0f;

    // Create the note card container
    Noesis::Ptr<Noesis::Grid> noteContainer = *new Noesis::Grid();
    noteContainer->SetWidth(noteCardWidth);
    noteContainer->SetHeight(noteCardHeight);
    noteContainer->SetClipToBounds(false); // Allow pin to extend beyond bounds

    // Create background image
    Noesis::Ptr<Noesis::Image> bgImage = *new Noesis::Image();
    Noesis::Ptr<Noesis::BitmapImage> bitmapSource = *new Noesis::BitmapImage();
    bitmapSource->SetUriSource(Noesis::Uri("GUI/Cards/Note_Card.png"));
    bgImage->SetSource(bitmapSource);
    bgImage->SetStretch(Noesis::Stretch_Fill);
    noteContainer->GetChildren()->Add(bgImage);

    // Store pin image reference for later color updates
    NoteCard tempCard;

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

    // Add pin image at top center (added last to ensure it renders on top of other elements)
    Noesis::Ptr<Noesis::Image> pinImage = *new Noesis::Image();
    pinImage->SetSource(pinBitmapImage);
    pinImage->SetWidth(24.0f);
    pinImage->SetHeight(24.0f);
    pinImage->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    pinImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    pinImage->SetMargin(
        Noesis::Thickness(0, -10.0f, 0, 0)); // Position pin centered in hit-box (moved down 2px)
    noteContainer->GetChildren()->Add(pinImage);
    tempCard.pin.pinImage = pinImage;

    // Position on canvas (centered)
    Noesis::Canvas::SetLeft(noteContainer, boardX - noteCardWidth / 2.0f);
    Noesis::Canvas::SetTop(noteContainer, boardY - noteCardHeight / 2.0f);

    // Add to caseboard
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        children->Add(noteContainer);
    }

    // Store reference (use tempCard's pin which has the image reference)
    NoteCard card;
    card.container = noteContainer;
    card.textBox = textBox;
    card.textLabel = nullptr;
    card.boardX = boardX;
    card.boardY = boardY;
    card.width = noteCardWidth;
    card.height = noteCardHeight;
    card.isEditing = true;
    card.savedText = "";
    card.pin = tempCard.pin;
    card.pin.pinOffsetY =
        -card.height / 2.0f + 2.0f; // Pin 2 pixels below top of card (centered in hit-box)
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

        // Ensure pin is added last (on top) - remove and re-add if it exists
        if (card.pin.pinImage) {
            children->Remove(card.pin.pinImage.GetPtr());
            children->Add(card.pin.pinImage);
        }
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

        float cardLeft = card.boardX - card.width / 2.0f;
        float cardTop = card.boardY - card.height / 2.0f;
        float cardRight = cardLeft + card.width;
        float cardBottom = cardTop + card.height;

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
        Noesis::Canvas::SetLeft(card.container, card.boardX - card.width / 2.0f);
        Noesis::Canvas::SetTop(card.container, card.boardY - card.height / 2.0f);
    }

    // Update connections
    RenderConnections(nullptr);
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

    // Update connections
    RenderConnections(nullptr);
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
    cardContainer->SetClipToBounds(false); // Allow pin to extend beyond bounds

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

    // Add pin image at top center (added last to ensure it renders on top of other elements)
    Noesis::Ptr<Noesis::Image> testimonyPinImage = Noesis::MakePtr<Noesis::Image>();
    testimonyPinImage->SetSource(pinBitmapImage);
    testimonyPinImage->SetWidth(24.0f);
    testimonyPinImage->SetHeight(24.0f);
    testimonyPinImage->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    testimonyPinImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    testimonyPinImage->SetMargin(
        Noesis::Thickness(0, -10.0f, 0, 0)); // Position pin centered in hit-box (moved down 2px)
    Noesis::Grid::SetRowSpan(testimonyPinImage, 2);
    cardContainer->GetChildren()->Add(testimonyPinImage);

    // Position card in board space (centered)
    Noesis::Canvas::SetLeft(cardContainer, boardX - testimony.width / 2.0f);
    Noesis::Canvas::SetTop(cardContainer, boardY - testimony.height / 2.0f);

    // Add to caseboard content
    caseboardContent->GetChildren()->Add(cardContainer);

    // Store in testimony cards list
    testimony.container = cardContainer;
    testimony.speakerLabel = speakerLabel;
    testimony.messageText = messageText;
    testimony.pin.pinImage = testimonyPinImage;
    testimony.pin.pinOffsetY =
        -testimony.height / 2.0f + 2.0f; // Pin 2 pixels below top (centered in hit-box)
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

    // Update connections
    RenderConnections(nullptr);
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

    // Case-file dimensions (folder shape - wider and shorter than note cards)
    const float fileWidth = 180.0f;
    const float fileHeight = 150.0f; // Shorter folder shape
    const float tabWidth = 20.0f;    // Width of the tab on the right edge
    const float tabHeight = 100.0f;  // ~67% of file height

    // Create the case-file container
    Noesis::Ptr<Noesis::Canvas> fileContainer = *new Noesis::Canvas();
    fileContainer->SetWidth(fileWidth + tabWidth);
    fileContainer->SetHeight(fileHeight);

    // Layer 1a: Yellow background for cover (with rounded right edge)
    Noesis::Ptr<Noesis::Border> coverBackground = *new Noesis::Border();
    coverBackground->SetWidth(fileWidth);
    coverBackground->SetHeight(fileHeight);
    coverBackground->SetMinWidth(fileWidth);
    coverBackground->SetMinHeight(fileHeight);
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
    Noesis::Canvas::SetLeft(rightTab, fileWidth - 0.5f);               // Overlap slightly
    Noesis::Canvas::SetTop(rightTab, fileHeight * 0.2f);               // Start 20% down
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
    Noesis::Canvas::SetLeft(leftTab, -tabWidth + 0.5f);               // Overlap slightly on left
    Noesis::Canvas::SetTop(leftTab, fileHeight * 0.2f);               // Start 20% down
    leftTab->SetVisibility(Noesis::Visibility_Collapsed);             // Hidden initially
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
    const float photoMarginH = 10.0f;      // Horizontal margin
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
    npcLabel->SetForeground(Noesis::MakePtr<Noesis::SolidColorBrush>(
        Noesis::Color(10, 15, 25))); // Very dark blue, nearly black
    npcLabel->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    npcLabel->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    npcLabel->SetTextAlignment(Noesis::TextAlignment_Center);
    npcLabel->SetEffect(nullptr); // No shadow
    npcLabel->SetFontWeight(Noesis::FontWeight_Bold);
    Noesis::Canvas::SetLeft(npcLabel, photoMarginH);
    Noesis::Canvas::SetTop(npcLabel, 8.0f);
    Noesis::Canvas::SetRight(npcLabel, photoMarginH + tabWidth);
    fileContainer->GetChildren()->Add(npcLabel);

    // Add pin image at top center (for case files, pins are at top, centered in hit-box)
    Noesis::Ptr<Noesis::Image> caseFilePinImage = *new Noesis::Image();
    caseFilePinImage->SetSource(pinBitmapImage);
    caseFilePinImage->SetWidth(24.0f);
    caseFilePinImage->SetHeight(24.0f);
    Noesis::Canvas::SetLeft(caseFilePinImage, (fileWidth - 24.0f) / 2.0f);
    Noesis::Canvas::SetTop(caseFilePinImage, -10.0f); // Moved down 2px to center in hit-box
    fileContainer->GetChildren()->Add(caseFilePinImage);

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
    caseFile.pin.pinImage = caseFilePinImage;
    caseFile.pin.pinOffsetY =
        -fileHeight / 2.0f + 2.0f; // Pin 2 pixels below top for case files (centered in hit-box)

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
                    valueText->SetEffect(nullptr);   // No shadow
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

    // Update connections
    RenderConnections(nullptr);
}

void CaseboardMode::StopDraggingCaseFile() {
    if (draggingCaseFileIndex >= 0) {
        wi::backlog::post("Stopped dragging case-file\n");
    }
    draggingCaseFileIndex = -1;
}

// Implementation of GetCardGeometry helper function
static bool GetCardGeometry(const CaseboardMode *mode, int cardType, int cardIndex, float &outX,
                            float &outY, float &outWidth, float &outHeight) {
    if (cardType == 0) { // Note
        const auto &cards = mode->GetNoteCards();
        if (cardIndex < 0 || cardIndex >= (int)cards.size())
            return false;
        const auto &card = cards[cardIndex];
        outX = card.boardX;
        outY = card.boardY;
        outWidth = card.width;
        outHeight = card.height;
        return true;
    } else if (cardType == 1) { // Photo
        const auto &cards = mode->GetPhotoCards();
        if (cardIndex < 0 || cardIndex >= (int)cards.size())
            return false;
        const auto &card = cards[cardIndex];
        outX = card.boardX;
        outY = card.boardY;
        outWidth = card.width;
        outHeight = card.height;
        return true;
    } else if (cardType == 2) { // Testimony
        const auto &cards = mode->GetTestimonyCards();
        if (cardIndex < 0 || cardIndex >= (int)cards.size())
            return false;
        const auto &card = cards[cardIndex];
        outX = card.boardX;
        outY = card.boardY;
        outWidth = card.width;
        outHeight = card.height;
        return true;
    } else if (cardType == 3) { // CaseFile
        const auto &cards = mode->GetCaseFiles();
        if (cardIndex < 0 || cardIndex >= (int)cards.size())
            return false;
        const auto &card = cards[cardIndex];
        outX = card.boardX;
        outY = card.boardY;
        outWidth = card.width;
        outHeight = card.height;
        return true;
    } else if (cardType == 4) { // CaseCard
        const auto &cards = mode->GetCaseCards();
        if (cardIndex < 0 || cardIndex >= (int)cards.size())
            return false;
        const auto &card = cards[cardIndex];
        outX = card.boardX;
        outY = card.boardY;
        outWidth = card.width;
        outHeight = card.height;
        return true;
    }
    return false;
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
    photoContainer->SetClipToBounds(false); // Allow pin to extend beyond bounds

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

    // Add pin image at top center (added last to ensure it renders on top of other elements)
    Noesis::Ptr<Noesis::Image> photoPinImage = *new Noesis::Image();
    photoPinImage->SetSource(pinBitmapImage);
    photoPinImage->SetWidth(24.0f);
    photoPinImage->SetHeight(24.0f);
    photoPinImage->SetVerticalAlignment(Noesis::VerticalAlignment_Top);
    photoPinImage->SetHorizontalAlignment(Noesis::HorizontalAlignment_Center);
    photoPinImage->SetMargin(
        Noesis::Thickness(0, -10.0f, 0, 0)); // Position pin centered in hit-box (moved down 2px)
    photoContainer->GetChildren()->Add(photoPinImage);

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
    card.pin.pinImage = photoPinImage;
    card.pin.pinOffsetY =
        -cardHeight / 2.0f + 2.0f; // Pin 2 pixels below top of card (centered in hit-box)
    photoCards.push_back(card);
}

// Pin and connection management methods
bool CaseboardMode::HitTestPin(int cardType, int cardIndex, float boardX, float boardY) {
    const float pinSize = 30.0f; // Pin hit area size
    float cardX, cardY, cardWidth, cardHeight;

    if (!GetCardGeometry(this, cardType, cardIndex, cardX, cardY, cardWidth, cardHeight)) {
        return false;
    }

    // Get pin offset to match visual position
    // Pins are 24x24 images with margin -12, so they extend 12px beyond the card edge
    float pinOffsetY;
    if (cardType == 4) { // case-card - pin 2 units above bottom
        pinOffsetY = cardHeight / 2.0f - 2.0f;
    } else { // Other cards (Note, Photo, Testimony) - pin 2 pixels below top (centered in hit-box)
        pinOffsetY = -cardHeight / 2.0f + 2.0f;
    }

    // Calculate pin X position
    // For case files (type 3), the pin is centered on the fileWidth (180px), not the full width
    // (180+20=200px) The tab adds 20px to the right, so the pin is shifted 10px left from cardX
    float pinCenterX = cardX;
    if (cardType == 3) { // CaseFile - adjust for tab offset
        const float tabWidth = 20.0f;
        pinCenterX = cardX - tabWidth / 2.0f;
    }
    float pinCenterY = cardY + pinOffsetY;

    // Check if mouse is within pin area
    float dx = boardX - pinCenterX;
    float dy = boardY - pinCenterY;
    float distSq = dx * dx + dy * dy;
    float radiusSq = (pinSize / 2.0f) * (pinSize / 2.0f);

    bool hit = distSq <= radiusSq;

    // Debug logging
    if (hit) {
        char buf[256];
        sprintf_s(buf, "Pin HIT! Type=%d Index=%d Mouse=(%.1f,%.1f) Pin=(%.1f,%.1f) Dist=%.1f\n",
                  cardType, cardIndex, boardX, boardY, pinCenterX, pinCenterY, std::sqrt(distSq));
        wi::backlog::post(buf);
    }

    return hit;
}

void CaseboardMode::StartConnection(int cardType, int cardIndex) {
    draggingConnection = true;
    dragStartCardType = cardType;
    dragStartCardIndex = cardIndex;

    float cardX, cardY, cardWidth, cardHeight;
    if (GetCardGeometry(this, cardType, cardIndex, cardX, cardY, cardWidth, cardHeight)) {
        float pinOffsetY;
        if (cardType == 4) { // case-card - pin 2 units above bottom
            pinOffsetY = cardHeight / 2.0f - 2.0f;
        } else { // Other cards (Note, Photo, Testimony, CaseFile) - pin 2 pixels below top
                 // (centered in hit-box)
            pinOffsetY = -cardHeight / 2.0f + 2.0f;
        }
        // Adjust X position for case files (pin centered on fileWidth, not full width with tab)
        dragConnectionX = cardX;
        if (cardType == 3) { // CaseFile
            const float tabWidth = 20.0f;
            dragConnectionX = cardX - tabWidth / 2.0f;
        }
        dragConnectionY = cardY + pinOffsetY;
    }

    // Create the drag preview path (will be updated in UpdateConnectionDrag)
    dragPreviewPath = *new Noesis::Path();
    dragPreviewPath->SetStrokeThickness(3.0f);
    dragPreviewPath->SetStroke(
        Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(220, 100, 100, 200)));
    dragPreviewPath->SetOpacity(0.7f);

    // Rebuild UI to include the drag preview in the correct position
    RebuildUIOrder();

    wi::backlog::post("Started connection drag\n");
}

void CaseboardMode::UpdateConnectionDrag(float boardX, float boardY) {
    if (!draggingConnection)
        return;

    dragConnectionX = boardX;
    dragConnectionY = boardY;

    // Update the drag preview path geometry
    if (dragPreviewPath) {
        float x1, y1, w1, h1;
        if (GetCardGeometry(this, dragStartCardType, dragStartCardIndex, x1, y1, w1, h1)) {
            // Pin center position
            float pinOffset;
            if (dragStartCardType == 4) { // case-card - pin 2 units above bottom
                pinOffset = h1 / 2.0f - 2.0f;
            } else { // Other cards - pin 2 pixels below top (centered in hit-box)
                pinOffset = -h1 / 2.0f + 2.0f;
            }
            // Adjust X position for case files
            float pinX1 = x1;
            if (dragStartCardType == 3) { // CaseFile
                const float tabWidth = 20.0f;
                pinX1 = x1 - tabWidth / 2.0f;
            }
            float pinY1 = y1 + pinOffset;
            float x2 = boardX;
            float y2 = boardY;

            // Calculate curve parameters
            float dist = std::sqrt((x2 - pinX1) * (x2 - pinX1) + (y2 - pinY1) * (y2 - pinY1));
            float sag = std::min(dist * 0.15f, 80.0f);
            float mx = (pinX1 + x2) / 2.0f;
            float my = (pinY1 + y2) / 2.0f + sag;

            // Update the path geometry
            Noesis::Ptr<Noesis::PathGeometry> geometry = *new Noesis::PathGeometry();
            Noesis::Ptr<Noesis::PathFigure> figure = *new Noesis::PathFigure();
            figure->SetStartPoint(Noesis::Point(pinX1, pinY1));

            Noesis::Ptr<Noesis::PointCollection> points = *new Noesis::PointCollection();
            const int numPoints = 17;
            for (int i = 1; i < numPoints; i++) {
                float t = (float)i / (float)(numPoints - 1);
                float px = (1 - t) * (1 - t) * pinX1 + 2 * (1 - t) * t * mx + t * t * x2;
                float py = (1 - t) * (1 - t) * pinY1 + 2 * (1 - t) * t * my + t * t * y2;
                points->Add(Noesis::Point(px, py));
            }

            Noesis::Ptr<Noesis::PolyLineSegment> segment = *new Noesis::PolyLineSegment();
            segment->SetPoints(points);
            figure->GetSegments()->Add(segment);
            geometry->GetFigures()->Add(figure);
            dragPreviewPath->SetData(geometry);
        }
    }

    // Update pin hovering states
    for (int i = 0; i < (int)noteCards.size(); i++) {
        bool wasHovering = noteCards[i].pin.hovering;
        noteCards[i].pin.hovering =
            (dragStartCardType != 0 || dragStartCardIndex != i) && HitTestPin(0, i, boardX, boardY);
        if (noteCards[i].pin.hovering != wasHovering) {
            UpdatePinColor(noteCards[i].pin.pinImage.GetPtr(), noteCards[i].pin.hovering);
        }
    }
    for (int i = 0; i < (int)photoCards.size(); i++) {
        bool wasHovering = photoCards[i].pin.hovering;
        photoCards[i].pin.hovering =
            (dragStartCardType != 1 || dragStartCardIndex != i) && HitTestPin(1, i, boardX, boardY);
        if (photoCards[i].pin.hovering != wasHovering) {
            UpdatePinColor(photoCards[i].pin.pinImage.GetPtr(), photoCards[i].pin.hovering);
        }
    }
    for (int i = 0; i < (int)testimonyCards.size(); i++) {
        bool wasHovering = testimonyCards[i].pin.hovering;
        testimonyCards[i].pin.hovering =
            (dragStartCardType != 2 || dragStartCardIndex != i) && HitTestPin(2, i, boardX, boardY);
        if (testimonyCards[i].pin.hovering != wasHovering) {
            UpdatePinColor(testimonyCards[i].pin.pinImage.GetPtr(), testimonyCards[i].pin.hovering);
        }
    }
    for (int i = 0; i < (int)caseFiles.size(); i++) {
        bool wasHovering = caseFiles[i].pin.hovering;
        caseFiles[i].pin.hovering =
            (dragStartCardType != 3 || dragStartCardIndex != i) && HitTestPin(3, i, boardX, boardY);
        if (caseFiles[i].pin.hovering != wasHovering) {
            UpdatePinColor(caseFiles[i].pin.pinImage.GetPtr(), caseFiles[i].pin.hovering);
        }
    }
    for (int i = 0; i < (int)caseCards.size(); i++) {
        bool wasHovering = caseCards[i].pin.hovering;
        caseCards[i].pin.hovering =
            (dragStartCardType != 4 || dragStartCardIndex != i) && HitTestPin(4, i, boardX, boardY);
        if (caseCards[i].pin.hovering != wasHovering) {
            UpdatePinColor(caseCards[i].pin.pinImage.GetPtr(), caseCards[i].pin.hovering);
        }
    }
}

void CaseboardMode::EndConnection(float boardX, float boardY) {
    if (!draggingConnection)
        return;

    // Check if we're over a pin (excluding the start card)
    int endCardType = -1;
    int endCardIndex = -1;

    for (int i = 0; i < (int)noteCards.size(); i++) {
        if ((dragStartCardType != 0 || dragStartCardIndex != i) &&
            HitTestPin(0, i, boardX, boardY)) {
            endCardType = 0;
            endCardIndex = i;
            break;
        }
    }
    if (endCardType < 0) {
        for (int i = 0; i < (int)photoCards.size(); i++) {
            if ((dragStartCardType != 1 || dragStartCardIndex != i) &&
                HitTestPin(1, i, boardX, boardY)) {
                endCardType = 1;
                endCardIndex = i;
                break;
            }
        }
    }
    if (endCardType < 0) {
        for (int i = 0; i < (int)testimonyCards.size(); i++) {
            if ((dragStartCardType != 2 || dragStartCardIndex != i) &&
                HitTestPin(2, i, boardX, boardY)) {
                endCardType = 2;
                endCardIndex = i;
                break;
            }
        }
    }
    if (endCardType < 0) {
        for (int i = 0; i < (int)caseFiles.size(); i++) {
            if ((dragStartCardType != 3 || dragStartCardIndex != i) &&
                HitTestPin(3, i, boardX, boardY)) {
                endCardType = 3;
                endCardIndex = i;
                break;
            }
        }
    }
    if (endCardType < 0) {
        for (int i = 0; i < (int)caseCards.size(); i++) {
            if ((dragStartCardType != 4 || dragStartCardIndex != i) &&
                HitTestPin(4, i, boardX, boardY)) {
                endCardType = 4;
                endCardIndex = i;
                break;
            }
        }
    }

    // If we found a valid end pin, create or toggle the connection
    if (endCardType >= 0 && endCardIndex >= 0) {
        // Check if connection already exists
        bool found = false;
        for (int i = 0; i < (int)connections.size(); i++) {
            Connection &conn = connections[i];
            if ((conn.cardAType == dragStartCardType && conn.cardAIndex == dragStartCardIndex &&
                 conn.cardBType == endCardType && conn.cardBIndex == endCardIndex) ||
                (conn.cardBType == dragStartCardType && conn.cardBIndex == dragStartCardIndex &&
                 conn.cardAType == endCardType && conn.cardAIndex == endCardIndex)) {
                // Connection exists, remove it
                connections.erase(connections.begin() + i);
                found = true;
                wi::backlog::post("Removed existing connection\n");
                break;
            }
        }

        if (!found) {
            // Create new connection
            Connection newConn;
            newConn.cardAType = dragStartCardType;
            newConn.cardAIndex = dragStartCardIndex;
            newConn.cardBType = endCardType;
            newConn.cardBIndex = endCardIndex;
            connections.push_back(newConn);

            char buf[256];
            sprintf_s(buf,
                      "Created connection: Card[%d,%d] <-> Card[%d,%d]. Total connections: %d\n",
                      dragStartCardType, dragStartCardIndex, endCardType, endCardIndex,
                      (int)connections.size());
            wi::backlog::post(buf);
        }
    } else {
        wi::backlog::post("EndConnection: No valid end pin found\n");
    }

    // Cancel the drag first (removes preview path)
    CancelConnection();

    // Then render permanent connections
    RenderConnections(nullptr);
}

void CaseboardMode::CancelConnection() {
    draggingConnection = false;
    dragStartCardType = -1;
    dragStartCardIndex = -1;

    // Remove the drag preview path
    if (dragPreviewPath) {
        dragPreviewPath.Reset();
        // Rebuild UI without the drag preview
        RebuildUIOrder();
    }

    // Clear all pin hovering states
    for (auto &card : noteCards)
        card.pin.hovering = false;
    for (auto &card : photoCards)
        card.pin.hovering = false;
    for (auto &card : testimonyCards)
        card.pin.hovering = false;
    for (auto &card : caseFiles)
        card.pin.hovering = false;
    for (auto &card : caseCards)
        card.pin.hovering = false;
}

void CaseboardMode::RemoveConnectionsForCard(int cardType, int cardIndex) {
    // Remove all connections involving this card
    for (int i = (int)connections.size() - 1; i >= 0; i--) {
        Connection &conn = connections[i];
        if ((conn.cardAType == cardType && conn.cardAIndex == cardIndex) ||
            (conn.cardBType == cardType && conn.cardBIndex == cardIndex)) {
            connections.erase(connections.begin() + i);
        }
    }
}

void CaseboardMode::RenderConnections(Noesis::Canvas *canvas) {
    if (!caseboardContent) {
        wi::backlog::post("RenderConnections: No caseboardContent available!\n");
        return;
    }

    // Clear old connection paths and debug rectangles from UI
    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (children) {
        for (auto &path : connectionPaths) {
            children->Remove(path);
        }
        for (auto &rect : debugRectangles) {
            children->Remove(rect.GetPtr());
        }
    }

    // Clear the connection paths and debug rectangles vectors
    connectionPaths.clear();
    debugRectangles.clear();

    char buf[256];
    sprintf_s(buf, "RenderConnections: Drawing %d permanent connections\n",
              (int)connections.size());
    wi::backlog::post(buf);

    // Draw all permanent connections
    for (size_t connIdx = 0; connIdx < connections.size(); connIdx++) {
        const Connection &conn = connections[connIdx];
        float x1, y1, w1, h1, x2, y2, w2, h2;
        if (!GetCardGeometry(this, conn.cardAType, conn.cardAIndex, x1, y1, w1, h1)) {
            wi::backlog::post("RenderConnections: Could not get geometry for cardA\n");
            continue;
        }
        if (!GetCardGeometry(this, conn.cardBType, conn.cardBIndex, x2, y2, w2, h2)) {
            wi::backlog::post("RenderConnections: Could not get geometry for cardB\n");
            continue;
        }

        // Calculate pin positions
        float pinOffsetA;
        if (conn.cardAType == 4) { // case-card - pin 2 units above bottom
            pinOffsetA = h1 / 2.0f - 2.0f;
        } else { // Other cards - pin 2 pixels below top (centered in hit-box)
            pinOffsetA = -h1 / 2.0f + 2.0f;
        }
        float pinOffsetB;
        if (conn.cardBType == 4) { // case-card - pin 2 units above bottom
            pinOffsetB = h2 / 2.0f - 2.0f;
        } else { // Other cards - pin 2 pixels below top (centered in hit-box)
            pinOffsetB = -h2 / 2.0f + 2.0f;
        }
        // Adjust X positions for case files (tab offset)
        float pinX1 = x1;
        if (conn.cardAType == 3) { // CaseFile
            const float tabWidth = 20.0f;
            pinX1 = x1 - tabWidth / 2.0f;
        }
        float pinX2 = x2;
        if (conn.cardBType == 3) { // CaseFile
            const float tabWidth = 20.0f;
            pinX2 = x2 - tabWidth / 2.0f;
        }
        float pinY1 = y1 + pinOffsetA;
        float pinY2 = y2 + pinOffsetB;

        char buf[256];
        sprintf_s(buf, "Drawing connection %zu: (%.1f,%.1f) -> (%.1f,%.1f)\n", connIdx, pinX1,
                  pinY1, pinX2, pinY2);
        wi::backlog::post(buf);

        // Calculate curve parameters
        float dist =
            std::sqrt((pinX2 - pinX1) * (pinX2 - pinX1) + (pinY2 - pinY1) * (pinY2 - pinY1));
        float sag = std::min(dist * 0.15f, 80.0f);
        float mx = (pinX1 + pinX2) / 2.0f;
        float my = (pinY1 + pinY2) / 2.0f + sag;

        // Create a Path with curved line using PathGeometry
        Noesis::Ptr<Noesis::Path> path = *new Noesis::Path();
        path->SetStrokeThickness(3.0f);
        path->SetStroke(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(200, 50, 50)));

        // Create geometry
        Noesis::Ptr<Noesis::PathGeometry> geometry = *new Noesis::PathGeometry();
        Noesis::Ptr<Noesis::PathFigure> figure = *new Noesis::PathFigure();

        // Set start point
        figure->SetStartPoint(Noesis::Point(pinX1, pinY1));

        // Create points for the curve
        Noesis::Ptr<Noesis::PointCollection> points = *new Noesis::PointCollection();
        const int numPoints = 17;
        for (int i = 1; i < numPoints; i++) {
            float t = (float)i / (float)(numPoints - 1);
            float px = (1 - t) * (1 - t) * pinX1 + 2 * (1 - t) * t * mx + t * t * pinX2;
            float py = (1 - t) * (1 - t) * pinY1 + 2 * (1 - t) * t * my + t * t * pinY2;
            points->Add(Noesis::Point(px, py));
        }

        // Create PolyLineSegment and add to figure
        Noesis::Ptr<Noesis::PolyLineSegment> segment = *new Noesis::PolyLineSegment();
        segment->SetPoints(points);
        figure->GetSegments()->Add(segment);

        // Add figure to geometry
        geometry->GetFigures()->Add(figure);

        // Set geometry on path
        path->SetData(geometry);

        // Add to our tracked list
        connectionPaths.push_back(path);
    }

#if 0
    // Add debug visualization for all cards
    // Draw card geometry rectangles (green)
    for (int type = 0; type <= 4; type++) {
        int count = 0;
        if (type == 0) count = (int)noteCards.size();
        else if (type == 1) count = (int)photoCards.size();
        else if (type == 2) count = (int)testimonyCards.size();
        else if (type == 3) count = (int)caseFiles.size();
        else if (type == 4) count = (int)caseCards.size();

        for (int i = 0; i < count; i++) {
            float x, y, w, h;
            if (GetCardGeometry(this, type, i, x, y, w, h)) {
                // Draw card bounds (green)
                Noesis::Ptr<Noesis::Rectangle> cardRect = *new Noesis::Rectangle();
                cardRect->SetWidth(w);
                cardRect->SetHeight(h);
                cardRect->SetStroke(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(0, 255, 0, 200)));
                cardRect->SetStrokeThickness(2.0f);
                cardRect->SetFill(nullptr);
                Noesis::Canvas::SetLeft(cardRect.GetPtr(), x - w / 2.0f);
                Noesis::Canvas::SetTop(cardRect.GetPtr(), y - h / 2.0f);
                debugRectangles.push_back(cardRect);

                // Draw pin hit-box (red circle as rectangle)
                const float pinSize = 30.0f;
                float pinOffsetY;
                if (type == 4) {
                    pinOffsetY = h / 2.0f - 2.0f;
                } else {
                    pinOffsetY = -h / 2.0f + 2.0f;
                }
                // Adjust X position for case files (tab offset)
                float pinCenterX = x;
                if (type == 3) { // CaseFile
                    const float tabWidth = 20.0f;
                    pinCenterX = x - tabWidth / 2.0f;
                }
                float pinCenterY = y + pinOffsetY;

                Noesis::Ptr<Noesis::Rectangle> pinRect = *new Noesis::Rectangle();
                pinRect->SetWidth(pinSize);
                pinRect->SetHeight(pinSize);
                pinRect->SetStroke(Noesis::MakePtr<Noesis::SolidColorBrush>(Noesis::Color(255, 0, 0, 200)));
                pinRect->SetStrokeThickness(2.0f);
                pinRect->SetFill(nullptr);
                Noesis::Canvas::SetLeft(pinRect.GetPtr(), pinCenterX - pinSize / 2.0f);
                Noesis::Canvas::SetTop(pinRect.GetPtr(), pinCenterY - pinSize / 2.0f);
                debugRectangles.push_back(pinRect);
            }
        }
    }
#endif

    // Rebuild the UI in correct order: background, connections, drag preview (if active), cards
    RebuildUIOrder();
}

void CaseboardMode::RebuildUIOrder() {
    if (!caseboardContent) {
        return;
    }

    Noesis::UIElementCollection *children = caseboardContent->GetChildren();
    if (!children) {
        return;
    }
    children->Clear();

    // Re-add in correct order:
    // 1. Background
    children->Add(background);

    // 2. Connection paths
    for (auto &path : connectionPaths) {
        children->Add(path);
    }

    // 3. Drag preview (if active)
    if (dragPreviewPath) {
        children->Add(dragPreviewPath);
    }

    // 4. All cards (in their original order)
    children->Add(caseCardsContent);

    for (auto &card : noteCards) {
        if (card.container) {
            children->Add(card.container);
        }
    }

    for (auto &card : photoCards) {
        if (card.container) {
            children->Add(card.container);
        }
    }

    for (auto &card : testimonyCards) {
        if (card.container) {
            children->Add(card.container);
        }
    }

    for (auto &file : caseFiles) {
        if (file.container) {
            children->Add(file.container);
        }
    }

    // 5. Debug rectangles (render on top)
    for (auto &rect : debugRectangles) {
        children->Add(rect);
    }
}
