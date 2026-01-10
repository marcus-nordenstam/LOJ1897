#pragma once

#include "WickedEngine.h"

#include <NsGui/BitmapImage.h>
#include <NsGui/Canvas.h>
#include <NsGui/Grid.h>
#include <NsGui/Image.h>
#include <NsGui/Panel.h>
#include <NsGui/ScaleTransform.h>
#include <NsGui/TextBlock.h>
#include <NsGui/TextBox.h>
#include <NsGui/TranslateTransform.h>

#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

// Forward declaration
class NoesisRenderPath;

// Caseboard system for evidence board management
class CaseboardMode {
  public:
    // Note cards (player-created notes)
    struct NoteCard {
        Noesis::Ptr<Noesis::Grid> container;
        Noesis::Ptr<Noesis::TextBox> textBox;
        Noesis::Ptr<Noesis::TextBlock> textLabel;
        float boardX = 0.0f;
        float boardY = 0.0f;
        bool isEditing = false;
        std::string savedText;
    };

    // Photo cards (captured evidence photos)
    struct PhotoCard {
        Noesis::Ptr<Noesis::Grid> container;
        Noesis::Ptr<Noesis::Image> photoImage;
        Noesis::Ptr<Noesis::TextBlock> label;
        float boardX = 0.0f;
        float boardY = 0.0f;
        std::string labelText;
        float width = 256.0f;
        float height = 320.0f;
    };

    // Case-file cards (NPC dossiers with multiple pages)
    struct CaseFile {
        Noesis::Ptr<Noesis::Canvas> container;
        Noesis::Ptr<Noesis::Image> coverPhoto;
        Noesis::Ptr<Noesis::TextBlock> npcNameLabel;
        std::vector<std::string> pages; // Page content (could be text or image paths)
        int currentPage = 0; // 0 = cover, 1+ = inside pages
        bool isOpen = false;
        float boardX = 0.0f;
        float boardY = 0.0f;
        std::string npcName;
        std::string photoFilename;
        float width = 200.0f; // 180 + 20 (tab)
        float height = 232.0f;
    };

    CaseboardMode() = default;
    ~CaseboardMode() = default;

    // Initialize with UI elements from XAML
    void Initialize(Noesis::Grid *panel, Noesis::Panel *content, Noesis::TextBlock *debugText,
                    HWND hwnd);

    // Shutdown and release UI references
    void Shutdown();

    // Enter caseboard mode
    void EnterCaseboardMode();

    // Exit caseboard mode
    void ExitCaseboardMode();

    // Update caseboard transforms based on current pan/zoom state
    void UpdateCaseboardTransforms();

    // Update caseboard debug text with current mouse position (board space) and zoom
    void UpdateCaseboardDebugText();

    // Handle caseboard zoom (mouse wheel)
    void CaseboardZoom(int x, int y, float delta);

    // Handle caseboard pan start (mouse down)
    void CaseboardPanStart(int x, int y);

    // Handle caseboard pan end (mouse up)
    void CaseboardPanEnd();

    // Clamp pan values to keep board visible within viewport
    void ClampCaseboardPan();

    // Handle caseboard pan move (mouse move while dragging)
    void CaseboardPanMove(int x, int y);

    // Add a note card at the center of the current view
    void AddNoteCard();

    // Finalize note card editing - save text and replace TextBox with TextBlock
    void FinalizeNoteCardEdit();

    // Handle click on caseboard - finalize any active note edit
    void OnCaseboardClick();

    // Start editing an existing note card (replace TextBlock with TextBox)
    void StartEditingNoteCard(int index);

    // Check if a board position is on a note card's draggable area (not the text area)
    // Returns the index of the note card, or -1 if not on any
    int HitTestNoteCardDragArea(float boardX, float boardY);

    // Check if a board position is on a photo card
    // Returns the index of the photo card, or -1 if not on any
    int HitTestPhotoCard(float boardX, float boardY);

    // Start dragging a note card
    void StartDraggingNoteCard(int index, float boardX, float boardY);

    // Start dragging a photo card
    void StartDraggingPhotoCard(int index, float boardX, float boardY);

    // Start dragging a case-file
    void StartDraggingCaseFile(int index, float boardX, float boardY);

    // Update dragged note card position
    void UpdateDraggingNoteCard(float boardX, float boardY);

    // Update dragged photo card position
    void UpdateDraggingPhotoCard(float boardX, float boardY);

    // Update dragged case-file position
    void UpdateDraggingCaseFile(float boardX, float boardY);

    // Stop dragging
    void StopDraggingNoteCard();

    // Stop dragging photo card
    void StopDraggingPhotoCard();

    // Stop dragging case-file
    void StopDraggingCaseFile();

    // Check if currently dragging a note card
    bool IsDraggingNoteCard() const { return draggingNoteCardIndex >= 0; }

    // Check if currently dragging a photo card
    bool IsDraggingPhotoCard() const { return draggingPhotoCardIndex >= 0; }

    // Check if currently dragging a case-file
    bool IsDraggingCaseFile() const { return draggingCaseFileIndex >= 0; }

    // Check if caseboard mode is active
    bool IsActive() const { return inCaseboardMode; }

    // Add a photo card to the caseboard with the image from file
    void AddPhotoCard(const std::string &photoFilename);

    // Add a case-file to the caseboard with NPC info and photo
    void AddCaseFile(const std::string &photoFilename, const std::string &npcName);

    // Handle click on case-file to open/navigate pages
    void HandleCaseFileClick(int caseFileIndex, float localX, float localY);

    // Check if a board position is on a case-file
    int HitTestCaseFile(float boardX, float boardY);

    // Get photo cards (for photo count display)
    const std::vector<PhotoCard> &GetPhotoCards() const { return photoCards; }
    
    // Get case files
    const std::vector<CaseFile> &GetCaseFiles() const { return caseFiles; }

    // Callback setter for mode change notifications
    using ModeChangeCallback = std::function<void(bool entering)>;
    void SetModeChangeCallback(ModeChangeCallback callback) { modeChangeCallback = callback; }

  private:
    // UI elements
    Noesis::Ptr<Noesis::Grid> caseboardPanel;
    Noesis::Ptr<Noesis::Panel> caseboardContent;
    Noesis::ScaleTransform *caseboardZoomTransform = nullptr;
    Noesis::TranslateTransform *caseboardPanTransform = nullptr;
    Noesis::Ptr<Noesis::TextBlock> caseboardDebugText;

    // Window handle for size calculations
    HWND windowHandle = nullptr;

    // Note cards, photo cards, and case files
    std::vector<NoteCard> noteCards;
    std::vector<PhotoCard> photoCards;
    std::vector<CaseFile> caseFiles;
    std::vector<Noesis::Ptr<Noesis::BitmapSource>> capturedPhotoTextures;

    // Editing state
    int editingNoteCardIndex = -1;
    int draggingNoteCardIndex = -1;
    int draggingPhotoCardIndex = -1;
    int draggingCaseFileIndex = -1;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;

    // State
    bool inCaseboardMode = false;

    // Pan/zoom state
    float caseboardZoom = 1.0f;
    float caseboardPanX = 0.0f;
    float caseboardPanY = 0.0f;
    bool caseboardPanning = false;
    POINT caseboardLastMousePos = {};
    POINT caseboardCurrentMousePos = {};

    // Visible/pannable area
    float caseboardVisibleHalfX = 3000.0f;
    float caseboardVisibleHalfY = 3000.0f;

    // Callback for mode changes
    ModeChangeCallback modeChangeCallback;
};
