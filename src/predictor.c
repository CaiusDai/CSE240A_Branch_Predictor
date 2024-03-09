//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"

//
// TODO:Student Information
//

const char *studentName = "Haoyu Liu && ZIJIE DAI";
const char *studentID   = "A59024691 && ";
const char *email       = "hal148@ucsd.edu && z2dai@ucsd.edu";


//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

/**********GShare**********/
uint8_t* gshare_table;  // Global history table
uint32_t ghistory;      // Global history register

/**********Tournament**********/

uint32_t globalHistory; // Global History Register
uint32_t* localHistoryTable;
uint8_t* localPredictionTable;
uint8_t* globalPredictionTable;
uint8_t* selectorTable;


//------------------------------------//
//       Gshare Helper Functions         //
//------------------------------------//

// Get the index of the GShare table
int gs_get_table_index(uint32_t pc){
  // Get the lower bits of the PC
  uint32_t pc_lower_bits = pc & ((1 << ghistoryBits) - 1);
  // XOR the lower bits of the PC with the global history
  return (ghistory ^ pc_lower_bits);
}

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//


// Initialize the predictor
//
void
init_predictor()
{
    switch (bpType) {
    case STATIC:
        return;
    case GSHARE:
        ghistory = 0;
        gshare_table = (uint8_t*)malloc((1<<ghistoryBits)*sizeof(uint8_t));
        for(int i = 0; i < (1<<ghistoryBits); i++){
          gshare_table[i] = WN;
        }
    case TOURNAMENT:
        globalHistory = 0;

        localHistoryTable = (uint32_t*)malloc((1 << pcIndexBits) * sizeof(uint32_t));
        memset(localHistoryTable, 0, (1 << pcIndexBits) * sizeof(uint32_t));

        localPredictionTable = (uint8_t*)malloc((1 << lhistoryBits) * sizeof(uint8_t));
        memset(localPredictionTable, NOTTAKEN, (1 << lhistoryBits) * sizeof(uint8_t));

        globalPredictionTable = (uint8_t*)malloc((1 << ghistoryBits) * sizeof(uint8_t));
        memset(globalPredictionTable, NOTTAKEN, (1 << ghistoryBits) * sizeof(uint8_t));

        selectorTable = (uint8_t*)malloc((1 << ghistoryBits) * sizeof(uint8_t));
        memset(selectorTable, 0, (1 << ghistoryBits) * sizeof(uint8_t));
    case CUSTOM:
    default:
        break;
    }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  uint32_t gs_index;// Table index used by GShare

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      gs_index = gs_get_table_index(pc);
      // If the counter value is SN or WN, predict NOTTAKEN,otherwise predict TAKEN
      return (gshare_table[gs_index] >= WT) ? TAKEN : NOTTAKEN;
    case TOURNAMENT:
        return tournament_predict(pc);
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

uint8_t
tournament_predict(uint32_t pc) {

    // Predictor 1: Simple BHT
    uint32_t pcIndex = pc & ((1 << pcIndexBits) - 1);
    uint32_t localHistory = localHistoryTable[pcIndex];
    uint32_t localPrediction = localPredictionTable[localHistory];

    // Predictor 2: Correlated Predictor
    uint32_t globalPrediction = globalPredictionTable[globalHistory];

    uint32_t selector = selectorTable[globalHistory];

    // Choose prediction based on selector value
    if (selector < 2) {
        return localPrediction;
    }
    else {
        return globalPrediction;
    }

}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{
    uint32_t gs_index,gs_mask;
    switch (bpType) {
    case STATIC:
        return;
    case GSHARE:
              // Calculate the index into the GShare table
              gs_index = gs_get_table_index(pc);
              // Update the counter based on the actual outcome
              if (outcome == TAKEN) {
                  gshare_table[gs_index] = (gshare_table[gs_index] < ST) ? gshare_table[gs_index] + 1 : ST;
              } else {
                  gshare_table[gs_index] = (gshare_table[gs_index] > SN) ? gshare_table[gs_index] - 1 : SN;
              }
              // Mask used to maintain the length of global history
              gs_mask = (1 << ghistoryBits) - 1;
              // Update global history using mask.
              ghistory = ((ghistory << 1) | outcome) & gs_mask;
              break;
    case TOURNAMENT:
        return tournament_train_predictor(pc, outcome);
    case CUSTOM:
    default:
        break;
    }

    return;
}

void
tournament_train_predictor(uint32_t pc, uint8_t outcome) {
    
    uint32_t pcIndex = pc & ((1 << pcIndexBits) - 1); // get the lower pcIndexBits bits of PC

    uint32_t localHistory = localHistoryTable[pcIndex];
    uint32_t localPrediction = localPredictionTable[localHistory];

    uint32_t globalPrediction = globalPredictionTable[globalHistory];

    uint32_t selector = selectorTable[globalHistory];

    // Update local history and prediction
    localHistoryTable[pcIndex] = ((localHistory << 1) | outcome) & ((1 << lhistoryBits) - 1);
    localPredictionTable[localHistory] = updatePrediction(localPrediction, outcome);

    // Update global history and prediction
    globalHistory = ((globalHistory << 1) | outcome) & ((1 << ghistoryBits) - 1);
    globalPredictionTable[globalHistory] = updatePrediction(globalPrediction, outcome);

    // Update selector based on performance of local and global predictions
    if (localPrediction != outcome && globalPrediction == outcome) {
        selectorTable[globalHistory] = updateSelector(selector, true); // Favor global
    }
    else if (localPrediction == outcome && globalPrediction != outcome) {
        selectorTable[globalHistory] = updateSelector(selector, false); // Favor local
    }


}

// Update the 2-bit saturating counter for predictions
uint8_t updatePrediction(uint8_t prediction, uint8_t outcome) {
    // If outcome is TAKEN
    if (outcome) {
        if (prediction < 3)
            prediction++;
    }
    // If outcome is NOT TAKEN
    else {
        if (prediction > 0)
            prediction--;
    }

    return prediction;
}

// Update the 2-bit saturating counter for the selector
uint8_t updateSelector(uint8_t selector, bool preferGlobal) {
    // If global predictor was better and selector is not strongly global (3)
    if (preferGlobal) {
        if (selector < 3) {
            selector++;
        }
    }
    // If local predictor was better and selector is not strongly local (0)
    else {
        if (selector > 0) {
            selector--;
        }
    }
    
    return selector;
}
