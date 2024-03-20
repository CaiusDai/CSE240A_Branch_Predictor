//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"
#include "perceptron.h"

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
uint32_t* localPHT; //localHistoryTable;
uint8_t* localBHT; //localPredictionTable;
uint8_t* globalBHT; //globalPredictionTable;
uint8_t* selectorTable;
uint8_t localOutcome, globalOutcome;

/**********Perceptron**********/

#define numWeights 28

// Total budget of the predictor
#define Budget (64256)

// Threshold values
#define ThetaMax (numWeights * 1.93 + 14)
#define ThetaMin ( -1*ThetaMax )

// Number of bits in a weight
#define BitsInWeight 8
#define MAXVAL 127
#define MINVAL -128

// calculation of entries of perceptron table
#define SizeOfPerceptron ((numWeights + 1) * BitsInWeight)
#define NumberPerceptron ((int)(Budget / SizeOfPerceptron))

int perceptron[NumberPerceptron][numWeights];
int bias[NumberPerceptron];
int GHR;
int index;
int output;
bool prediction;



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
        break;
    case TOURNAMENT:
        globalHistory = 0;

        localPHT = (uint32_t*)malloc((1 << pcIndexBits) * sizeof(uint32_t));
        memset(localPHT, 0, (1 << pcIndexBits) * sizeof(uint32_t));

        localBHT = (uint8_t*)malloc((1 << lhistoryBits) * sizeof(uint8_t));
        memset(localBHT, WN, (1 << lhistoryBits) * sizeof(uint8_t));

        globalBHT = (uint8_t*)malloc((1 << ghistoryBits) * sizeof(uint8_t));
        memset(globalBHT, WN, (1 << ghistoryBits) * sizeof(uint8_t));

        selectorTable = (uint8_t*)malloc((1 << ghistoryBits) * sizeof(uint8_t));
        memset(selectorTable, WN, (1 << ghistoryBits) * sizeof(uint8_t));
        break;
    case CUSTOM:
        init_perceptron();
    default:
        break;
    }
}

void
init_perceptron(){
      for(int i = 0; i < NumberPerceptron; i++)
        for (int j = 0 ; j <= numWeights ; j++)
            perceptron[i][j] = 0;
      GHR = 0;
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
        return perceptron_predict(pc);
    default:
        break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

uint8_t
tournament_predict(uint32_t pc) {

    // Predictor 1: Local
    uint32_t pcIndex = pc & ((1 << pcIndexBits) - 1);
    uint32_t localHistory = localPHT[pcIndex];
    uint32_t localPrediction = localBHT[localHistory];
    localOutcome = ((localPrediction == WN || localPrediction == SN) ? NOTTAKEN : TAKEN);

    // Predictor 2: Global
    uint32_t globalBHTIndex = (globalHistory) & ((1 << ghistoryBits) - 1);
    uint32_t globalPrediction = globalBHT[globalBHTIndex];
    globalOutcome = ((globalPrediction == WN || globalPrediction == SN) ? NOTTAKEN : TAKEN);

    uint32_t selector = selectorTable[globalBHTIndex];

    // Choose prediction based on selector value
    if (selector < 2) {
        return globalOutcome;
    }
    else {
        return localOutcome;
    }

}

uint8_t
perceptron_predict(uint32_t pc){
      prediction = NOTTAKEN;

      // hash function to map pc to a row in the perceptron table
      index = pc % NumberPerceptron;
      int bit = 1;
      output = bias[index];
      for(int i = 0; i < numWeights; i++){
          int result;

    	  if((GHR & bit) == 0)
    		result = -1;
    	  else
    		result = 1;

    	  output += perceptron[index][i] * result;

    	  bit = bit << 1;
    	}

      // making a prediction
      if( output >= 0 )
    	prediction = 1;
      else
    	prediction = 0;

     return prediction;
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
        return perceptron_train_predictor(pc, outcome);
    default:
        break;
    }

    return;
}

void
tournament_train_predictor(uint32_t pc, uint8_t outcome) {
    uint8_t* selector = &selectorTable[globalHistory];
    // Update selector based on performance of local and global predictions
    if (localOutcome != outcome && globalOutcome == outcome) {
        updatePrediction(selector, NOTTAKEN);
    }
    else if (localOutcome == outcome && globalOutcome != outcome) {
        updatePrediction(selector, TAKEN);
    }

    uint32_t pcIndex = pc & ((1 << pcIndexBits) - 1);
    uint32_t localBHTIndex = localPHT[pcIndex];
    uint8_t* localPrediction = &localBHT[localBHTIndex];
    uint8_t* globalPrediction = &globalBHT[globalHistory];

    // Update local history and prediction
    updatePrediction(localPrediction, outcome);
    localPHT[pcIndex] = ((localBHTIndex << 1) | outcome) & ((1 << lhistoryBits) - 1);

    // Update global history and prediction
    updatePrediction(globalPrediction, outcome);
    globalHistory = ((globalHistory << 1) | outcome) & ((1 << ghistoryBits) - 1);

    return;
}

void
perceptron_train_predictor(uint32_t pc, uint8_t outcome){

     // update bias
     int bia;
     if(outcome == TAKEN){
        bia = 1;
     }
     else{
        bia = -1;
     }

     int bit = 1;

     // updating weights and bias
     if( outcome != prediction || (output < ThetaMax && output > ThetaMin) ){

        // checking whether bias is within a value that is 8 bit long
        if ( bias[index] > MINVAL && bias[index] < MAXVAL) {
            // updating bias
            bias[index] = bias[index] + bia;
        }

        for(int i = 0; i < numWeights; i++){
            int result;
            // checking whether correlation is positive or negative
            if( ( ( (GHR & bit) != 0) && (outcome == 1) ) ||  ( ((GHR & bit) == 0) && (outcome == 0) ) )
                result = 1;
            else
                result = -1;

            // checking whether ith weight at index is within a value that is 8 bit long
            if ( perceptron[index][i] > MINVAL && perceptron[index][i] < MAXVAL ) {
                // updating weight
                perceptron[index][i] = perceptron[index][i] + result;
            }

            //finding out bit-th bit of GHR
            bit = bit << 1;
        }

     }
     GHR = ( GHR << 1 ) ;
     GHR = (GHR | outcome);

}

//------------------------------------//
//       Tournament Helper Functions  //
//------------------------------------//

// Update the 2-bit saturating counter for predictions
void updatePrediction(uint8_t *prediction, uint8_t outcome) {
    // If outcome is TAKEN
    if (outcome == TAKEN) {
        if (*prediction < 3)
            (*prediction)++;
    }
    // If outcome is NOT TAKEN
    else {
        if (*prediction > 0)
            (*prediction)--;
    }
}
