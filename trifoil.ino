
enum signalStates {INERT, SEND, RESPOND, RESOLVE};
byte signalState = INERT;
// order of signal types is important, higher has a higher override
enum signalTypes {SOURCE2SINK, PUSH, BLOOM};
byte signalType = SOURCE2SINK;
bool isSource = false;

byte myData = 0;

void setup() {
  randomize();
}

void resetToIdle() {
  signalState = INERT;
  signalType = SOURCE2SINK;
}

void loop() {


  FOREACH_FACE(f) {
    if(!isValueReceivedOnFaceExpired(f)) {
      byte neighborSignalState = getLastValueReceivedOnFace(f) & 3;
      byte neighborSignalType = (getLastValueReceivedOnFace(f) >> 2) & 3;
      byte neighborData = (getLastValueReceivedOnFace(f) >> 4) & 3;

      // check if neighbor has a signal type that should override ours
      if (neighborSignalType > signalType) {
        signalType = neighborSignalType;
        signalState = neighborSignalState;
        if (signalType == PUSH) {
          // TODO: check if we should be in SEND or RESPOND
          // TODO: if we are in RESPOND, check if push was valid
        }
        myData = neighborData;
      }
    }
  }

  bool buttonWasPressed = buttonPressed();
  switch (signalType) {
    case SOURCE2SINK:
      switch (signalType) {
        case IDLE:
          if (buttonWasPressed) {
            // TODO: check if we are source or sink
            signalType = SEND;
            isSource = true;
          }
          break;
        case SEND:
          // TODO: check if neighbors are send
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
          // TODO: check if neighbor is respond
          // TODO: broadcast send to correct neighbor with push color bit
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
          break;
        case RESOLVE:
          break;
        default:
          // there must have been something wrong
          resetToIdle();
          break;
      }
      break;
  }
}