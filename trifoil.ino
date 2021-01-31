enum propagationStates {
    INERT,
    SEND,
    RESPOND,
    RESOLVE
};
byte propagationState = INERT;
// order of signal types is important, higher has a higher override
enum signalModes {
    SOURCE2SINK,
    PUSH,
    BLOOM
};
byte signalMode = SOURCE2SINK;

byte incomingPropagationStates[6] = { INERT, INERT, INERT, INERT, INERT, INERT };
byte incomingSignalModes[6] = { SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK };
byte incomingNeighborData[6] = { 0, 0, 0, 0, 0, 0};

enum bloomActions {
    UNDO,
    TURN_CHANGE,
    RESET
};
byte myData = 0;

byte lastPushColorBitReceived = 0;

Timer sharedTimer;
const float sinkBroadcastSendTimeout = 0.5;
const int moveCancelTimeout = 10;

byte pips[] = { 0, 1, 0, 0, 0, 0 };
bool currentTurnColor = false; // turn color true -> pip[f] = 2

void setup() {
randomize();
}

void resetToIdle() {
    propagationState = INERT;
    signalMode = SOURCE2SINK;
}

void loop() {

    FOREACH_FACE(f) {
        if (!isValueReceivedOnFaceExpired(f) && getLastValueReceivedOnFace(f) >= 0 ) {
            incomingPropagationStates[f] = getLastValueReceivedOnFace(f) & 3;
            incomingSignalModes[f] = (getLastValueReceivedOnFace(f) >> 2) & 3;
            incomingNeighborData[f] = (getLastValueReceivedOnFace(f) >> 4) & 3;

            // check if neighbor has a signal type that should override ours
            if (incomingSignalModes[f] > signalMode && incomingPropagationStates[f] == SEND) {
                propagationState = incomingPropagationStates[f];
                signalMode = incomingSignalModes[f];
                if (signalMode == PUSH) {
                    // TODO: read the push color bit and save it in lastPushColorBitReceived
                    // TODO: check if we should be in SEND or RESPOND
                    // TODO: if we are in RESPOND, check if push was valid
                }
                myData = incomingNeighborData[f];
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
        myData = TURN_CHANGE;
    }
    if (buttonWasLongPressed) {
        // reset
        signalMode = BLOOM;
        propagationState = SEND;
        myData = RESET;
    }
    // TODO: respond to triple click for undo
    switch (signalMode) {
    case SOURCE2SINK:
        switch (propagationState) {
        case INERT:
            if (!buttonWasPressed) {
             
              FOREACH_FACE(f) {
                setValueSentOnFace(0, f);
              }
            }
            else {
                // We're source if none of our neighbors are broadcasting SEND
                bool isSource = false;
                byte faceGettingSend = 0;
                FOREACH_FACE(f) {
                    if (incomingPropagationStates[f] == SEND) {
                        isSource = true;
                        break;
                    }
                }
                if (isSource) {
                  propagationState = SEND;
                }
                else {
                  propagationState = RESPOND;
                }
                
                // // As source, we need to check for which neighbor is broadcasting SEND and
                // // try to input a move on that side. If a pip is already present on that
                // // face then attempt a push.
                // if (isSource) {
                //     // Is the attempted action a push because that face has a pip already there?
                //     if (pips[faceGettingSend] != 0) {
                //         signalMode = PUSH;
                //     } else {
                //         // TODO Add making the move
                //         if (currentTurnColor) {
                //             pips[faceGettingSend] = 1;
                //         } else {
                //             pips[faceGettingSend] = 2;
                //         }
                        
                //         resetToIdle();
                //     }
                    
                //     // If the player doesn't make a move for a certain amount of time, then
                //     // cancel it.
                //     // TODO act on this timer
                //     // sharedTimer.set(moveCancelTimeout);
                // // As sink, we need to keep broadcasting SEND for 0.5 seconds and then change to INERT
                // } else {
                //     sharedTimer.set(sinkBroadcastSendTimeout);
                //     while (true) {
                //         if (sharedTimer.isExpired()) {
                //             resetToIdle();
                //           break;
                //             // TODO broadcast state? Maybe handle at top of switch.
                //         }
                //     }
                // }
            }
            break;
        case SEND:
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
        case SEND:
            // TODO: check if neighbor is respond. if they are and push success is true, write change to pips (using lastPushColorBitReceived to get the push color)
            // TODO: broadcast send to correct neighbor with myPushColorBit
            break;
        case RESPOND:
            // TODO: check if any neighbor is resolve
            // TODO: broadcast respond to both neighbors with push success bit
            break;
        case RESOLVE:
            // TODO: check if neighbors are all resolve
            // TODO: broadcast resolve
            break;
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    case BLOOM:
        // TODO: broadcast SEND to all neighbors with myData
        byte toBroadcast = 0;
        toBroadcast += propagationState;
        toBroadcast += BLOOM << 2;
        toBroadcast += myData << 4;
        FOREACH_FACE(f) {
          setValueSentOnFace(toBroadcast, f);
        }
        switch (propagationState) {
        case SEND:
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
                    break;
                case TURN_CHANGE:
                    currentTurnColor = !currentTurnColor;
                    break;
                case RESET:
                    FOREACH_FACE(f) { pips[f] = 0; };
                default:
                    break;
                }
            }
            // TODO: check if all neighbors are SEND. if so, switch to resolve and do the action stored in myData
            break;
        case RESOLVE:
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
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    }

    FOREACH_FACE(f) {
        if (pips[f] == 0) {
            if (currentTurnColor) {
                setColorOnFace(dim(RED, 60), f);
            }
            else {
                setColorOnFace(dim(CYAN, 60), f);
            }
        }
        if (pips[f] == 1) {
            setColorOnFace(RED, f);
        }
        if (pips[f] == 2) {
            setColorOnFace(CYAN, f);
        }
    }
}