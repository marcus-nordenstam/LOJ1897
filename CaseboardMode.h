#pragma once

#include "WickedEngine.h"

#include <NsGui/BitmapImage.h>
#include <NsGui/Border.h>
#include <NsGui/Canvas.h>
#include <NsGui/Grid.h>
#include <NsGui/Image.h>
#include <NsGui/Panel.h>
#include <NsGui/Path.h>
#include <NsGui/ScaleTransform.h>
#include <NsGui/StackPanel.h>
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
    // Pin data - each card has one pin for connections
    struct Pin {
        bool hovering = false;
        float pinOffsetY = 0.0f;             // Offset from card center
        Noesis::Ptr<Noesis::Image> pinImage; // Reference to pin visual
    };

    // Connection between two cards
    struct Connection {
        int cardAIndex = -1; // Index in respective card array
        int cardBIndex = -1;
        int cardAType = -1; // 0=Note, 1=Photo, 2=Testimony, 3=CaseFile, 4=CaseCard
        int cardBType = -1;
    };

    // Note cards (player-created notes)
    struct NoteCard {
        Noesis::Ptr<Noesis::Grid> container;
        Noesis::Ptr<Noesis::TextBox> textBox;
        Noesis::Ptr<Noesis::TextBlock> textLabel;
        float boardX = 0.0f;
        float boardY = 0.0f;
        float width = 180.0f;
        float height = 232.0f;
        bool isEditing = false;
        std::string savedText;
        Pin pin;
    };

    // Photo cards (captured evidence photos)
    struct PhotoCard {
        Noesis::Ptr<Noesis::Grid> container;
        Noesis::Ptr<Noesis::Image> photoImage;
        Noesis::Ptr<Noesis::TextBlock> label;
        float boardX = 0.0f;
        float boardY = 0.0f;
        std::string labelText;
        float width = 180.0f;  // Same as black cards
        float height = 225.0f; // Maintains 4:5 aspect ratio
        Pin pin;
    };

    // Testimony cards (recorded NPC dialogue)
    struct TestimonyCard {
        Noesis::Ptr<Noesis::Grid> container;
        Noesis::Ptr<Noesis::TextBlock> speakerLabel; // Speaker name at top
        Noesis::Ptr<Noesis::TextBlock> messageText;  // Message content
        float boardX = 0.0f;
        float boardY = 0.0f;
        std::string speaker;
        std::string message;
        float width = 180.0f;  // Same as black cards
        float height = 232.0f; // Same as black cards
        Pin pin;
    };

    // Field in a case-file page (label-value pair)
    struct CaseFileField {
        std::string label;
        std::string value;
    };

    // Page in a case-file (contains multiple fields)
    struct CaseFilePage {
        std::vector<CaseFileField> fields;
    };

    // Case cards (static black cards: VICTIM, SUSPECT, MOTIVE, etc.)
    struct CaseCard {
        Noesis::Ptr<Noesis::Grid> container; // Grid containing the card image and pin
        Noesis::Ptr<Noesis::Image> cardImage; // The card image itself
        float boardX = 0.0f;
        float boardY = 0.0f;
        float width = 180.0f;  // Same as other cards
        float height = 232.0f;  // Same as other cards
        Pin pin;
    };

    // Case-file cards (NPC dossiers with multiple pages)
    struct CaseFile {
        Noesis::Ptr<Noesis::Canvas> container;
        Noesis::Ptr<Noesis::Border> coverBackground;   // Yellow background for cover
        Noesis::Ptr<Noesis::Image> pageBackground;     // Notepad background for content pages
        Noesis::Ptr<Noesis::Grid> photoImageContainer; // Container for the cover photo
        Noesis::Ptr<Noesis::Image> coverPhoto;
        Noesis::Ptr<Noesis::TextBlock> npcNameLabel;
        Noesis::Ptr<Noesis::StackPanel> pageContentPanel; // Panel for displaying page fields
        Noesis::Ptr<Noesis::Border> leftTab; // Tab for going back (visible when currentPage > 0)
        Noesis::Ptr<Noesis::TextBlock> leftTabArrow;
        std::vector<CaseFilePage> pages; // Pages with fields
        int currentPage = 0;             // 0 = cover, 1+ = inside pages
        bool isOpen = false;
        float boardX = 0.0f;
        float boardY = 0.0f;
        std::string npcName;
        std::string photoFilename;
        float width = 200.0f; // 180 + 20 (tab)
        float height = 150.0f; // Shorter folder shape
        Pin pin;
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

    // Render connections on a canvas (should be called to draw connections UNDER cards)
    void RenderConnections(Noesis::Canvas *canvas);

    // Rebuild UI element order: background, connections, drag preview, cards
    void RebuildUIOrder();

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

    // Check if a board position is on a testimony card
    // Returns the index of the testimony card, or -1 if not on any
    int HitTestTestimonyCard(float boardX, float boardY);

    // Start dragging a note card
    void StartDraggingNoteCard(int index, float boardX, float boardY);

    // Start dragging a photo card
    void StartDraggingPhotoCard(int index, float boardX, float boardY);

    // Start dragging a testimony card
    void StartDraggingTestimonyCard(int index, float boardX, float boardY);

    // Start dragging a case-file
    void StartDraggingCaseFile(int index, float boardX, float boardY);

    // Update dragged note card position
    void UpdateDraggingNoteCard(float boardX, float boardY);

    // Update dragged photo card position
    void UpdateDraggingPhotoCard(float boardX, float boardY);

    // Update dragged testimony card position
    void UpdateDraggingTestimonyCard(float boardX, float boardY);

    // Update dragged case-file position
    void UpdateDraggingCaseFile(float boardX, float boardY);

    // Stop dragging
    void StopDraggingNoteCard();

    // Stop dragging photo card
    void StopDraggingPhotoCard();

    // Stop dragging testimony card
    void StopDraggingTestimonyCard();

    // Stop dragging case-file
    void StopDraggingCaseFile();

    // Check if currently dragging a note card
    bool IsDraggingNoteCard() const { return draggingNoteCardIndex >= 0; }

    // Check if currently dragging a photo card
    bool IsDraggingPhotoCard() const { return draggingPhotoCardIndex >= 0; }

    // Check if currently dragging a testimony card
    bool IsDraggingTestimonyCard() const { return draggingTestimonyCardIndex >= 0; }

    // Check if currently dragging a case-file
    bool IsDraggingCaseFile() const { return draggingCaseFileIndex >= 0; }

    // Check if currently editing a note card
    bool IsEditingNoteCard() const { return editingNoteCardIndex >= 0; }

    // Check if caseboard mode is active
    bool IsActive() const { return inCaseboardMode; }

    // Add a photo card to the caseboard with the image from file
    void AddPhotoCard(const std::string &photoFilename);

    // Add a testimony card to the caseboard with speaker name and message
    void AddTestimonyCard(const std::string &speaker, const std::string &message);

    // Add a case-file to the caseboard with NPC info and photo
    void AddCaseFile(const std::string &photoFilename, const std::string &npcName);

    // Handle click on case-file right tab to navigate forward
    void HandleCaseFileClick(int caseFileIndex, float localX, float localY);

    // Handle click on case-file left tab to navigate backward
    void HandleCaseFileLeftTabClick(int caseFileIndex);

    // Update the visual content of a case-file to show current page
    void UpdateCaseFilePageDisplay(CaseFile &file);

    // Populate case-file with default pages/fields
    void PopulateCaseFilePages(CaseFile &file);

    // Check if a board position is on a case-file
    int HitTestCaseFile(float boardX, float boardY);

    // Get photo cards (for photo count display)
    const std::vector<PhotoCard> &GetPhotoCards() const { return photoCards; }

    // Get case files
    const std::vector<CaseFile> &GetCaseFiles() const { return caseFiles; }

    // Get note cards
    const std::vector<NoteCard> &GetNoteCards() const { return noteCards; }

    // Get testimony cards
    const std::vector<TestimonyCard> &GetTestimonyCards() const { return testimonyCards; }

    // Get case cards
    const std::vector<CaseCard> &GetCaseCards() const { return caseCards; }

    // Pin and connection management
    bool HitTestPin(int cardType, int cardIndex, float boardX, float boardY);
    void StartConnection(int cardType, int cardIndex);
    void UpdateConnectionDrag(float boardX, float boardY);
    void EndConnection(float boardX, float boardY);
    void CancelConnection();
    bool IsDraggingConnection() const { return draggingConnection; }
    void RemoveConnectionsForCard(int cardType, int cardIndex);

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

    Noesis::Ptr<Noesis::UIElement> background;
    Noesis::Ptr<Noesis::UIElement> caseCardsContent;

    // Window handle for size calculations
    HWND windowHandle = nullptr;

    // Note cards, photo cards, testimony cards, case cards, and case files
    std::vector<NoteCard> noteCards;
    std::vector<PhotoCard> photoCards;
    std::vector<TestimonyCard> testimonyCards;
    std::vector<CaseCard> caseCards;
    std::vector<CaseFile> caseFiles;
    std::vector<Noesis::Ptr<Noesis::BitmapSource>> capturedPhotoTextures;

    // Connections between cards
    std::vector<Connection> connections;

    // Pin brush for rendering
    Noesis::Ptr<Noesis::BitmapImage> pinBitmapImage;

    // Editing state
    int editingNoteCardIndex = -1;
    int draggingNoteCardIndex = -1;
    int draggingPhotoCardIndex = -1;
    int draggingTestimonyCardIndex = -1;
    int draggingCaseFileIndex = -1;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;

    // Connection dragging state
    bool draggingConnection = false;
    int dragStartCardType = -1; // 0=Note, 1=Photo, 2=Testimony, 3=CaseFile, 4=CaseCard
    int dragStartCardIndex = -1;
    float dragConnectionX = 0.0f;
    float dragConnectionY = 0.0f;
    Noesis::Ptr<Noesis::Path> dragPreviewPath; // Reusable path for drag preview

    // Rendered connection paths (separate from logical connections)
    std::vector<Noesis::Ptr<Noesis::Path>> connectionPaths;

    // Debug visualization
    std::vector<Noesis::Ptr<Noesis::FrameworkElement>> debugRectangles;

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
