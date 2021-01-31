enum signalStates {
    INERT,
    SEND,
    RESPOND,
    RESOLVE
};
byte signalState = INERT;
// order of signal types is important, higher has a higher override
enum signalTypes {
    SOURCE2SINK,
    PUSH,
    BLOOM
};
byte signalType = SOURCE2SINK;
bool isSource = false;

byte incomingSignalStates[6] = { INERT, INERT, INERT, INERT, INERT, INERT };
byte incomingSignalTypes[6] = { SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK, SOURCE2SINK };
byte incomingNeighborData[6] = {};

enum bloomActions {
    UNDO,
    TURN_CHANGE,
    RESET
};
byte myData = 0;

byte lastPushColorBitReceived = 0;

Timer sharedTimer;
const float sinkBroadcastSendTimeout = 0.5;

byte pips[] = { 0, 0, 0, 0, 0, 0 };

void setup() {
randomize();
}

void resetToIdle() {
    signalState = INERT;
    signalType = SOURCE2SINK;
}

void loop() {

    FOREACH_FACE(f) {
        if (!isValueReceivedOnFaceExpired(f)) {
            incomingSignalStates[f] = getLastValueReceivedOnFace(f) & 3;
            incomingSignalTypes[f] = (getLastValueReceivedOnFace(f) >> 2) & 3;
            incomingNeighborData[f] = (getLastValueReceivedOnFace(f) >> 4) & 3;

            // check if neighbor has a signal type that should override ours
            if (incomingSignalTypes[f] > signalType) {
                signalState = incomingSignalStates[f];
                signalType = incomingSignalTypes[f];
                if (signalType == PUSH) {
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
    bool buttonWasPressed = buttonPressed();
    switch (signalType) {
    case SOURCE2SINK:
        switch (signalType) {
        case INERT:
            if (buttonWasPressed) {
                // TODO: check if we are source or sink
                // We're source if none of our neighbors are broadcasting SEND
                isSource = false;
                FOREACH_FACE(f) {
                    if (incomingSignalStates[f] == SEND) {
                        isSource = true;
                        break;
                    }
                }
                signalType = SEND;
                
                // As source, we need to check for which neighbor is broadcasting SEND and
                // try to input a move on that side. If a pip is already present on that
                // face then attempt a push.
                if (isSource) {
                    
                // As sink, we need to keep broadcasting SEND for 0.5 seconds and then change to INERT
                } else {
                    sharedTimer.set(sinkBroadcastSendTimeout);
                    while (true) {
                        if (sharedTimer.isExpired()) {
                            resetToIdle();
                            // TODO broadcast state
                        }
                    }
                }
            }
            if (buttonWasDoubleClicked) {
                // turn change
            }
            if (buttonWasLongPressed) {
                // reset
                signalType = BLOOM;
                signalState = SEND;
                myData = RESET;
            }
            // TODO: respond to double click for turn change
            // TODO: respond to triple click for undo
            // TODO: respond to long press for reset
            break;
        case SEND:
            // TODO: check if neighbors are send. if they are, write pip and go to resolve or PUSH SEND
            //       if we are going to PUSH SEND make sure to write to myPushColorBit
            // TODO: broadcast send
            break;
        case RESOLVE:
            // TODO: broadcast resolve
            break;
        default:
            // there must have been something wrong
            resetToIdle();
            break;
        }
        break;
    case PUSH:
        switch (signalType) {
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
        switch (signalState) {
        case SEND:
            // TODO: broadcast SEND to all neighbors with myData
            byte toBroadcast = 0;
            toBroadcast += SEND;
            toBroadcast += BLOOM << 2;
            toBroadcast += myData << 4;
            FOREACH_FACE(f) {
                setValueSentOnFace(toBroadcast, f);
            }
            bool isAnyNeighborNotSendOrResolve = false;
            FOREACH_FACE(f) {
                if (!isValueReceivedOnFaceExpired(f)
                    && (incomingSignalStates[f] < SEND // if it's SEND or RESOLVE we're OK
                        || incomingSignalTypes[f] != BLOOM)) {
                    isAnyNeighborNotSendOrResolve = true;
                    break;
                }
            }
            if (!isAnyNeighborNotSendOrResolve) {
                signalState = RESOLVE;
                switch (myData) {
                case UNDO:
                    break;
                case TURN_CHANGE:
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
                    && (!(incomingSignalStates[f] == INERT || incomingSignalStates[f] == RESOLVE)
                        || incomingSignalTypes[f] != BLOOM)) {
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
            setColorOnFace(dim(WHITE, 0), f);
        }
        if (pips[f] == 1) {
            setColorOnFace(RED, f);
        }
        if (pips[f] == 2) {
            setColorOnFace(CYAN, f);
        }
    }
}