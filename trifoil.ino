

// ************************************************************************************
// ************************************************************************************
//
// NOTES FOR SPENCER
// if you are the sink, we should go into RESPOND. this should allow us to not
// have more global state
// check the updated TODOs in the SOURCE2SINK SEND and SOURCE2SINK RESPOND states
// the state machine doc has also been updated
//
// ************************************************************************************
// ************************************************************************************


#define PENDING_CHANGE_PULSE_WIDTH 3000

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
bool wasPushSource = false;

Timer sharedTimer;
const float sinkBroadcastSendTimeout = 0.5;
const int moveCancelTimeout = 10;

byte pushDirection = 0;

byte pips[] = { 2, 1, 0, 0, 0, 0 };
byte lastPips[] = {0, 0, 0, 0, 0, 0}; // for undo
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
                    }
                    else if (pips[pushDirection] == 0) { // 1 pip displaced
                      pips[pushDirection] = pips[f];
                      pips[f] = lastPushColorBitReceived + 1;
                      myData = 2 + (myData & 1); // set the success bit to 1
                      propagationState = RESPOND;
                    }
                    else if (!isValueReceivedOnFaceExpired(pushDirection)) { // both pips and neighbor exists
                      myData = (pips[pushDirection] - 1); // set the color bit
                      propagationState = SEND;
                    }
                    else { // both pips and neighbor does not exist
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
        myData = TURN_CHANGE;
    }
    if (buttonWasLongPressed) {
        // reset
        signalMode = BLOOM;
        propagationState = SEND;
        //myData = RESET;
        myData = UNDO;
    }
    if (buttonWasTriplePressed) {
      if (pips[pushDirection] > 0 && !isValueReceivedOnFaceExpired(pushDirection)) {
        wasPushSource = true;
        signalMode = PUSH;
        propagationState = SEND;
        myData = (pips[pushDirection] - 1); // set the color bit
      }
    }
    // TODO: respond to triple click for undo
    byte toBroadcast = 0;
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
                } //probably don't add more code below this
                
                
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
                }
                else { // we're only in PUSH SEND and not source if both push pips are filled
                  pips[pushDirection] = pips[(pushDirection + 3) % 6];
                  pips[(pushDirection + 3) % 6] = lastPushColorBitReceived + 1;
                }
              }
              propagationState = RESPOND;
            }
            break;
        case RESPOND:
            toBroadcast = 0;
            toBroadcast += RESPOND;
            toBroadcast += PUSH << 2;
            toBroadcast += myData << 4;
            setValueSentOnAllFaces(toBroadcast);
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
        case RESOLVE:
            toBroadcast = 0;
            toBroadcast += RESOLVE;
            toBroadcast += PUSH << 2;
            toBroadcast += myData << 4;
            setValueSentOnAllFaces(toBroadcast);
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
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    case BLOOM:
        // TODO: broadcast SEND to all neighbors with myData
        toBroadcast = 0;
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
                    FOREACH_FACE(f) {
                        pips[f] = lastPips[f];
                    }
                    break;
                case TURN_CHANGE:
                    FOREACH_FACE(f) {
                        lastPips[f] = pips[f];
                    }
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

    bool arePipsChanged = false;
    FOREACH_FACE(f) {
        if (pips[f] != lastPips[f]) {
            arePipsChanged = true;
            break;
        }
    }
    FOREACH_FACE(f) {
        if (pips[f] == 0) {
            byte brightness = 60;
            if (arePipsChanged) {
                brightness = sin8_C(map(millis() % PENDING_CHANGE_PULSE_WIDTH, 0, PENDING_CHANGE_PULSE_WIDTH, 0, 255));
            }
            if (currentTurnColor) {
                setColorOnFace(dim(RED, brightness), f);
            }
            else {
                setColorOnFace(dim(CYAN, brightness), f);
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