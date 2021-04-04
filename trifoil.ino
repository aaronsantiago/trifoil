
#define PENDING_CHANGE_PULSE_WIDTH 800
#define CHAIN_PW 200
#define SPINNER_PW 700

#define CHAIN_DARK_FRAMES 10
#define CHAIN_DARK_COOLDOWN_FRAMES 30
#define CHAIN_SEND_FRAMES 3

#define TURN_COLOR_BRIGHTNESS 73

// this must be less than the cooldown frames to work intuitively
#define CHAIN_UNDARK_FRAMES 20

#define CHAIN_START_CHANCE 2000

// used for color blending
#define RED_R 255
#define RED_G 30
#define RED_B 0

#define BLUE_R 20
#define BLUE_G 40
#define BLUE_B 225

#define RED_COLOR makeColorRGB(RED_R, RED_G, RED_B)
#define BLUE_COLOR makeColorRGB(BLUE_R, BLUE_G, BLUE_B)

enum propagationStates { INERT, SEND, RESPOND, RESOLVE };
byte propagationState = INERT;
// order of signal types is important, higher has a higher override
enum signalModes { SOURCE2SINK, PUSH, BLOOM };
byte signalMode = SOURCE2SINK;

byte incomingPropagationStates[6] = { INERT, INERT, INERT, INERT, INERT, INERT };
byte incomingSignalModes[6] = { SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK };
byte incomingNeighborData[6] = { 0, 0, 0, 0, 0, 0};
byte chainTimer[6] = { 0, 0, 0, 0, 0, 0};
bool chainSending[6] = { false, false, false, false, false, false };
bool chainStartedFromExternal[6] = { false, false, false, false, false, false };

byte animBuffer[6] = { 0, 0, 0, 0, 0, 0 };

enum bloomActions { UNDO, TURN_CHANGE_RED, TURN_CHANGE_BLUE, RESET };
byte myData = 0;

byte lastPushColorBitReceived = 0;
bool wasPushSource = false;
bool isSource = true;

Timer sharedTimer;
const int sinkBroadcastSendTimeout = 500;
const int moveCancelTimeout = 10;
bool haveSetTimer = false;

byte pushDirection = 0;

byte pips[] = { 0, 0, 0, 0, 0, 0 };
byte lastPips[] = {0, 0, 0, 0, 0, 0}; // for undo
bool currentTurnColor = true; // turn color true -> pip[f] = 2

bool justWoke = hasWoken();

void setup() {
    randomize();
}

void resetToIdle() {
    propagationState = INERT;
    signalMode = SOURCE2SINK;
    isSource = false;
    wasPushSource = false;
    haveSetTimer = false;
}

void broadcastOnAllFaces() {
    byte broadcastVal = propagationState;
    broadcastVal += signalMode << 2;
    broadcastVal += myData << 4;
    setValueSentOnAllFaces(broadcastVal);
}

int calcRGB(int c, int b) {
    return c * b / 255;
}

void loop() {

    FOREACH_FACE(f) {
        if (!isValueReceivedOnFaceExpired(f)) {
            incomingPropagationStates[f] = getLastValueReceivedOnFace(f) & 3;
            incomingSignalModes[f] = (getLastValueReceivedOnFace(f) >> 2) & 3;

            incomingNeighborData[f] = (getLastValueReceivedOnFace(f) >> 4) & 3;

            // check if neighbor has a signal type that should override ours, and is trying to SEND to us
            if (incomingSignalModes[f] > signalMode && incomingPropagationStates[f] == SEND) {
                propagationState = incomingPropagationStates[f];
                signalMode = incomingSignalModes[f];
                myData = incomingNeighborData[f];
                if (signalMode == PUSH) {
                    lastPushColorBitReceived = incomingNeighborData[f] & 1;
                    pushDirection = (f + 3) % 6;

                    if (pips[f] == 0) { // regular push
                        pips[f] = lastPushColorBitReceived + 1;
                        myData = 2 + (myData & 1); // set the success bit
                        propagationState = RESPOND;
                    } else if (pips[pushDirection] == 0) { // 1 pip displaced
                        pips[pushDirection] = pips[f];
                        pips[f] = lastPushColorBitReceived + 1;
                        myData = 2 + (myData & 1); // set the success bit to 1
                        propagationState = RESPOND;
                    } else if (!isValueReceivedOnFaceExpired(pushDirection)) { // both pips and neighbor exists
                        myData = (pips[pushDirection] - 1); // set the color bit
                        propagationState = SEND;
                    } else { // both pips and neighbor does not exist
                        myData = 0 + (myData & 1); // set the success bit to 0
                        propagationState = RESPOND;
                    }
                }
            }
        }
    }

    bool buttonWasTriplePressed = buttonMultiClicked();
    bool buttonWasLongPressed = buttonLongPressed();
    bool buttonWasDoubleClicked = buttonDoubleClicked();
    bool buttonWasPressed = buttonSingleClicked();
    if (buttonWasDoubleClicked) {
        // TODO: respond to double click for turn change
        signalMode = BLOOM;
        propagationState = SEND;
        myData = currentTurnColor ? TURN_CHANGE_RED : TURN_CHANGE_BLUE;
    }
    if (buttonWasLongPressed) {
        // reset
        signalMode = BLOOM;
        propagationState = SEND;
        myData = RESET;
    }
    if (buttonWasTriplePressed) {
        signalMode = BLOOM;
        propagationState = SEND;
        myData = UNDO;
    }
    byte toBroadcast = 0;
    switch (signalMode) {
    case SOURCE2SINK:
        switch (propagationState) {
        case INERT:
            if (!buttonWasPressed) {

                FOREACH_FACE(f) {
                    // send color using mydata to each neighbor so that we can animate chains
                    if (!chainSending[f]) {
                        setValueSentOnFace(pips[f] << 4, f);
                    } else {
                        setValueSentOnFace(3 << 4, f);
                    }
                }
            // This justWoke check prevents us from displaying the spinner when
            // a user single clicks a blink to wake a group of blinks from sleep.
            } else if (!justWoke) {
                // We're source if none of our neighbors are broadcasting SEND
                isSource = true;
                FOREACH_FACE(f) {
                    if (incomingPropagationStates[f] == SEND) {
                        isSource = false;
                        break;
                    }
                }
                if (isSource) {
                    propagationState = SEND;
                } else {
                    propagationState = SEND;
                } //probably don't add more code below this
                broadcastOnAllFaces();
            }
            break;
        case SEND:
            // As source, we need to check for which neighbor is broadcasting SEND (previously RESPOND) and
            // try to input a move on that side. If a pip is already present on that
            // face then attempt a push.
            if (isSource) {
                // If the source is clicked again before we get a response from our neighbors
                // then cancel the move. Note that we don't call buttonSingleClicked() again here
                // because the call above has already consumed the click.
                if (buttonWasPressed) {
                    resetToIdle();
                    broadcastOnAllFaces();
                }

                // Check which neighbor is broadcasting RESPOND
                byte neighborBroadcastingRespond = 6; // no response
                FOREACH_FACE(f) {
                    if (!isValueReceivedOnFaceExpired(f) && incomingPropagationStates[f] == SEND) {
                        neighborBroadcastingRespond = f;
                        break;
                    }
                }
                if (neighborBroadcastingRespond == 6) {
                    break;
                }
                // Is the attempted action a push because that face has a pip already there?
                if (pips[neighborBroadcastingRespond] != 0) {
                    // Are we trying to push a color that doesn't belong to us? If so go back to idle
                    if ((currentTurnColor && pips[neighborBroadcastingRespond] == 1) ||
                            (!currentTurnColor && pips[neighborBroadcastingRespond] == 2)) {
                        resetToIdle();
                        broadcastOnAllFaces();
                    } else {
                        pushDirection = neighborBroadcastingRespond;
                        wasPushSource = true;
                        signalMode = PUSH;
                        propagationState = SEND;
                        myData = (pips[pushDirection] - 1); // set the color bit
                    }

                } else {
                    if (currentTurnColor) {
                        pips[neighborBroadcastingRespond] = 2;
                    } else {
                        pips[neighborBroadcastingRespond] = 1;
                    }
                    resetToIdle();
                    broadcastOnAllFaces();
                }
                // If the player doesn't make a move for a certain amount of time, then
                // cancel it.
                // TODO act on this timer
                // sharedTimer.set(moveCancelTimeout);
                // As sink, we need to keep broadcasting SEND for 0.5 seconds and then change to INERT
            } else {
                if (!haveSetTimer) {
                    sharedTimer.set(sinkBroadcastSendTimeout);
                    haveSetTimer = true;
                }
                // This should be broadcasting RESPOND
                broadcastOnAllFaces();
                if (sharedTimer.isExpired()) {
                    resetToIdle();
                    haveSetTimer = false;
                    broadcastOnAllFaces();
                }
            }

            // TODO: check if any neighbors are RESPOND. if they are, then trigger a move.
            // (write pip and go to INERT or, save push direction and go to PUSH SEND)
            //       if we are going to PUSH SEND make sure to write to myPushColorBit
            // TODO: check if timeout is finished, if so go to INERT
            // TODO: broadcast send to all neighbors
            break;
        case RESPOND:
            // TODO: check if timeout is finished, if so go to INERT
            // TODO: broadcast send only to source
            break;
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    case PUSH:
        switch (propagationState) {
        case SEND: {
            toBroadcast = 0;
            toBroadcast += SEND;
            toBroadcast += PUSH << 2;
            toBroadcast += myData << 4;
            setValueSentOnAllFaces(0);
            setValueSentOnFace(toBroadcast, pushDirection);

            bool isPushNeighborRespond = false;
            if (!isValueReceivedOnFaceExpired(pushDirection)
                    && incomingPropagationStates[pushDirection] == RESPOND
                    && incomingSignalModes[pushDirection] == PUSH) {
                isPushNeighborRespond = true;
            }
            if (isPushNeighborRespond) {
                // set our success bit to match the incoming one
                myData = (incomingNeighborData[pushDirection] & 2) + (myData & 1);
                if (myData >> 1 > 0) {
                    if (wasPushSource) {
                        pips[pushDirection] = 0;
                    } else { // we're only in PUSH SEND and not source if both push pips are filled
                        pips[pushDirection] = pips[(pushDirection + 3) % 6];
                        pips[(pushDirection + 3) % 6] = lastPushColorBitReceived + 1;
                    }
                }
                propagationState = RESPOND;
            }
            break;
        }
        case RESPOND: {
            broadcastOnAllFaces();
            // we're not SEND ing so it won't matter if we send to all
            // this lets us skip keeping track of which neighbors to RESPOND to

            bool areAllPUSHNeighborsRespondOrResolve = true;
            FOREACH_FACE(f) {
                if (!isValueReceivedOnFaceExpired(f)
                        && (!(incomingPropagationStates[f] == RESPOND || incomingPropagationStates[f] == RESOLVE)
                            && incomingSignalModes[f] == PUSH)) {
                    areAllPUSHNeighborsRespondOrResolve = false;
                    break;
                }
            }
            if (areAllPUSHNeighborsRespondOrResolve) {
                propagationState = RESOLVE;
            }
            break;
        }
        case RESOLVE: {
            broadcastOnAllFaces();
            // we're not SEND ing so it won't matter if we send to all
            // this lets us skip keeping track of which neighbors to send RESOLVE to

            bool areAllNeighborsResolveOrInert = true;
            FOREACH_FACE(f) {
                if (!isValueReceivedOnFaceExpired(f)
                        && !(incomingPropagationStates[f] == INERT || incomingPropagationStates[f] == RESOLVE)) {
                    areAllNeighborsResolveOrInert = false;
                    break;
                }
            }
            if (areAllNeighborsResolveOrInert) {
                resetToIdle();
            }
            break;
        }
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    case BLOOM:
        // TODO: broadcast SEND to all neighbors with myData
        broadcastOnAllFaces();
        switch (propagationState) {
        case SEND: {
            bool isAnyNeighborNotSendOrResolve = false;
            FOREACH_FACE(f) {
                if (!isValueReceivedOnFaceExpired(f)
                        && (incomingPropagationStates[f] < SEND // if it's SEND or RESOLVE we're OK
                            || incomingSignalModes[f] != BLOOM)) {
                    isAnyNeighborNotSendOrResolve = true;
                    break;
                }
            }
            if (!isAnyNeighborNotSendOrResolve) {
                propagationState = RESOLVE;
                switch (myData) {
                case UNDO:
                    FOREACH_FACE(f) {
                        pips[f] = lastPips[f];
                    }
                    break;
                case TURN_CHANGE_RED:
                    FOREACH_FACE(f) {
                        lastPips[f] = pips[f];
                    }
                    currentTurnColor = false;
                    break;
                case TURN_CHANGE_BLUE:
                    FOREACH_FACE(f) {
                        lastPips[f] = pips[f];
                    }
                    currentTurnColor = true;
                    break;
                case RESET:
                    FOREACH_FACE(f) {
                        pips[f] = 0;
                        lastPips[f] = 0;
                    };
                    currentTurnColor = true;
                default:
                    break;
                }
            }
            // TODO: check if all neighbors are SEND. if so, switch to resolve and do the action stored in myData
            break;
        }
        case RESOLVE: {
            bool isAnyNeighborNotResolveOrInert = false;
            FOREACH_FACE(f) {
                if (!isValueReceivedOnFaceExpired(f)
                        // if neighbors are inert or resolve we are OK
                        && (!(incomingPropagationStates[f] == INERT || incomingPropagationStates[f] == RESOLVE))) {
                    isAnyNeighborNotResolveOrInert = true;
                    break;
                }
            }
            if (!isAnyNeighborNotResolveOrInert) {
                resetToIdle();
            }
            break;
        }
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    }

    bool arePipsChanged = false;
    byte blueCount = 0;
    byte redCount = 0;
    FOREACH_FACE(f) {
        if (pips[f] != lastPips[f]) {
            arePipsChanged = true;
        }
        if (pips[f] == 1) redCount++;
        if (pips[f] == 2) blueCount++;

    }
    byte current_R[6] = {0, 0, 0, 0, 0, 0};
    byte current_G[6] = {0, 0, 0, 0, 0, 0};
    byte current_B[6] = {0, 0, 0, 0, 0, 0};
    FOREACH_FACE(f) {

        int brightness = TURN_COLOR_BRIGHTNESS;
        if (pips[f] == 0) {
            // Animation saying "this blink has been changed"
            if (arePipsChanged) {
                brightness = map(sin8_C(
                                     map(millis() % PENDING_CHANGE_PULSE_WIDTH, 0, PENDING_CHANGE_PULSE_WIDTH, 0, 255)
                                 ), 0, 255, 0, 60);
            }
        } else {
            // If the pip is active, animate
            brightness = 255;

            // Check if the neighbor pip is telling us that a chain pulse is happening
            if (incomingNeighborData[f] == 3 && !isValueReceivedOnFaceExpired(f)) {
                if (chainTimer[f] == 0) {
                    chainStartedFromExternal[f] = true;
                    chainTimer[f] = 1;
                }
            }

            // Get the count of "adjacent" pips of the same color including the neighbor
            byte currentCount = redCount;
            if (pips[f] == 2) currentCount = blueCount;
            if (pips[f] > 0 && random(CHAIN_START_CHANCE) < 10 && chainTimer[f] == 0
                    && ((currentCount == 1) != (incomingNeighborData[f] != pips[f] && !isValueReceivedOnFaceExpired(f)))) {
                chainTimer[f] = 1;
            }

            // Chain pulse is active
            if (chainTimer[f] > 0) {
                chainTimer[f] += 1;

                // Ensure we stop sending to our neighbor if we're past the specified number of send frames
                if (chainTimer[f] > CHAIN_DARK_FRAMES + CHAIN_SEND_FRAMES) {
                    chainSending[f] = false;
                }
                // If we're done with the entire chain pulse, reset
                if (chainTimer[f] > CHAIN_DARK_COOLDOWN_FRAMES + CHAIN_DARK_FRAMES) {
                    chainTimer[f] = 0;
                    chainStartedFromExternal[f] = false;
                }
                // Smoothly animate the brightness up
                if (chainTimer[f] > CHAIN_DARK_FRAMES) {
                    brightness = map(chainTimer[f] - CHAIN_DARK_FRAMES, 0, CHAIN_UNDARK_FRAMES, 100, 255);
                }
                // Smoothly animate the brightness down
                if (chainTimer[f] <= CHAIN_DARK_FRAMES && chainTimer[f] != 0) {
                    brightness = 255 - map(chainTimer[f], 0, CHAIN_DARK_FRAMES, 0, 155);
                }

                bool neighborSendAvailable = false;
                // Once we reach exactly CHAIN_DARK_FRAMES, pass the chain to a neighbor or adjacent link
                if (chainTimer[f] == CHAIN_DARK_FRAMES) {
                    int chance = currentCount - 2; //inclusive random, exclude self

                    // Add neighbor if they are the same color
                    if (pips[f] == incomingNeighborData[f] && !isValueReceivedOnFaceExpired(f) && !chainStartedFromExternal[f]) {
                        chance += 1;
                        neighborSendAvailable = true;
                    }
                    byte pick = random(chance);
                    // If there are no neighbors or adjacent links to use
                    if (chance >= 0) {

                        // Find all valid adjacent links
                        byte i = 0;
                        FOREACH_FACE(g) {
                            if (f == g) continue;

                            if (pips[g] == pips[f]) {
                                // Is this the one we randomly chose?
                                if (i++ >= pick) {
                                    // Is this one not already chain pulsing?
                                    // If it's already chain pulsing, we'll choose the next one instead
                                    if (chainTimer[g] == 0) {
                                        chainTimer[g] = 1;
                                        // Now that we chose a link to pass the pulse to, put all the other adjacent
                                        // links on cooldown
                                        FOREACH_FACE(h) {
                                            if (h == g || h == f) continue;
                                            if (chainTimer[h] == 0 && pips[h] == pips[f]) {
                                                chainTimer[h] = CHAIN_DARK_FRAMES + CHAIN_UNDARK_FRAMES + 1;
                                            }
                                        }
                                        i = 255;
                                        break;
                                    }
                                }
                            }
                        }
                        // if we didn't choose any adjacent links, send to our neighbor
                        if (i < 255 && neighborSendAvailable) {
                            chainSending[f] = true;
                        }
                    }
                }
            }
        }

        if (pips[f] == 1 || pips[f] == 0 && !currentTurnColor) {
            current_R[f] = calcRGB(RED_R, brightness);
            current_G[f] = calcRGB(RED_G, brightness);
            current_B[f] = calcRGB(RED_B, brightness);
        }
        if (pips[f] == 2 || pips[f] == 0 && currentTurnColor) {
            current_R[f] = calcRGB(BLUE_R, brightness);
            current_G[f] = calcRGB(BLUE_G, brightness);
            current_B[f] = calcRGB(BLUE_B, brightness);
        }
    }
    if (propagationState == SEND && signalMode == SOURCE2SINK && isSource) {
        byte index = (byte)(map(millis() % SPINNER_PW, 0, SPINNER_PW, 0, 5));
        animBuffer[index] = 255;
    }
    FOREACH_FACE(f) {
        current_R[f] = max(current_R[f], animBuffer[f]);
        current_G[f] = max(current_G[f], animBuffer[f]);
        current_B[f] = max(current_B[f], animBuffer[f]);
        animBuffer[f] = (byte)(animBuffer[f] * .9);
    }
    if (propagationState == RESOLVE && signalMode == BLOOM) {
        FOREACH_FACE(f) {
            if(myData == TURN_CHANGE_RED) animBuffer[f] = 125;
            if(myData == TURN_CHANGE_BLUE) animBuffer[f] = 125;
            if(myData == RESET) animBuffer[f] = 255;
            if(myData == UNDO) animBuffer[f] = 55;
        }
    }
    FOREACH_FACE(f) {
        setColorOnFace(makeColorRGB(current_R[f], current_G[f], current_B[f]), f);
    }

    justWoke = hasWoken();
}
