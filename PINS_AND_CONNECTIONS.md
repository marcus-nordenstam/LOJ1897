# Pins and Connections Implementation

This document describes the pins and connections system added to the caseboard, based on the original Slate implementation from E:/UnrealProjects/TechDemo/Source.

## Overview

The implementation adds the ability to connect cards on the caseboard using pins, similar to the original Slate version but adapted for the Noesis UI framework.

## Key Differences from Slate Version

1. **Connections Render UNDER Cards**: Unlike Slate where connections are drawn over cards, here they render underneath
2. **Hover Indication**: Instead of animating the pin with a sine wave, we change the mouse cursor to a hand and update the pin's visual appearance
3. **Framework Adaptation**: Adapted from Slate's custom rendering to Noesis's element-based UI system

## Components Added

### 1. Pin Structure (`CaseboardMode.h`)
Each card type now has a `Pin` member with:
- `hovering`: Boolean indicating if mouse is over the pin
- `pinOffsetY`: Y-offset from card center (negative for top, positive for bottom)
- `pinImage`: Reference to the visual pin image element

### 2. Connection Structure (`CaseboardMode.h`)
```cpp
struct Connection {
    int cardAIndex, cardBIndex;  // Indices in respective arrays
    int cardAType, cardBType;    // 0=Note, 1=Photo, 2=Testimony, 3=CaseFile
};
```

### 3. Pin Image (`GUI/Cards/Pin.png`)
A simple red pushpin graphic (64x64 pixels) generated as a placeholder. You can replace this with your own PNG.

## Visual Behavior

### Pin Placement
- **Note Cards, Photo Cards, Testimony Cards**: Pin at TOP center
- **Case Files**: Pin at BOTTOM center

### Hover States
- **Cursor**: Changes to hand cursor when hovering over pins
- **Visual**: Pin opacity/appearance changes when hovering (can be enhanced with color tinting)

### Connection Appearance
- **Permanent Connections**: Red curved lines (bezier curves with sag)
- **Drag Preview**: Semi-transparent red line while dragging from a pin
- **Rendering**: Lines drawn using multiple Noesis Line segments to approximate curves

## User Interaction

### Creating a Connection
1. Click on a pin to start dragging
2. Drag to another card's pin
3. Release to create connection
4. If connection already exists, it will be removed (toggle behavior)

### During Drag
- Mouse cursor changes to hand when over a valid target pin
- Live preview line shows from source pin to mouse position
- Target pins highlight when hovered

### Connection Deletion
- Dragging between two already-connected pins removes the connection

## Implementation Details

### Key Methods

#### `HitTestPin(cardType, cardIndex, boardX, boardY)`
Tests if board position is within a pin's hit area (16px radius).

#### `StartConnection(cardType, cardIndex)`
Initiates connection dragging from a specific card's pin.

#### `UpdateConnectionDrag(boardX, boardY)`
Updates the drag preview and pin hover states during mouse movement.

#### `EndConnection(boardX, boardY)`
Completes or cancels the connection based on whether mouse is over a valid target pin.

#### `RenderConnections(canvas)`
Redraws all connections on the connections canvas layer:
- Clears existing line elements
- Draws all permanent connections as bezier curves
- Draws temporary drag line if active

### Connection Rendering Algorithm

Connections use quadratic Bezier curves with these properties:
- **Start/End**: Pin positions on source and target cards
- **Control Point**: Midpoint with vertical sag (15% of distance, max 80px)
- **Segments**: 16 line segments approximate the curve
- **Thickness**: 3px lines
- **Color**: RGB(200, 50, 50) for permanent, semi-transparent for preview

### Integration Points

The system integrates into existing mouse handling:
- `CaseboardPanStart`: Checks for pin clicks before other interactions
- `CaseboardPanMove`: Updates connection dragging and pin hover states
- `CaseboardPanEnd`: Completes or cancels connection
- Card dragging methods: Call `RenderConnections()` to update line positions

### Connections Canvas

A dedicated canvas (`connectionsCanvas`) is created at initialization and inserted as the first child of the caseboard content panel, ensuring connections render under all cards.

## Future Enhancements

Potential improvements you might consider:
1. **Pin Colors**: Add color-coding based on card type or connection state
2. **Connection Colors**: Allow different colors for different types of relationships
3. **Connection Labels**: Add text labels to connections
4. **Connection Thickness**: Vary thickness based on "strength" or importance
5. **Better Pin Hover**: Use Noesis effects or color brushes for more dramatic hover feedback
6. **Connection Deletion**: Add explicit delete interaction (e.g., right-click on line)
7. **Persistence**: Save/load connections with caseboard state

## Testing

To test the implementation:
1. Enter caseboard mode
2. Create some cards (notes, photos, testimony, case files)
3. Click on a pin (small circle at top/bottom of card)
4. Drag to another card's pin and release
5. Verify red curved line appears connecting the cards
6. Cards should stay connected even when dragged around
7. Click same two pins again to remove connection

## Notes

- The implementation uses `GetCardGeometry()` helper to abstract card position/size queries across all card types
- Pin images are added during card creation and stored as references for hover updates
- Connection rendering is triggered automatically when cards move or connections change
- The system handles all four card types uniformly through type/index pairs
