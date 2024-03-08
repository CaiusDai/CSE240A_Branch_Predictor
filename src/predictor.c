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
const char *studentName = "ZIJIE DAI";
const char *studentID   = "PID";
const char *email       = "z2dai@ucsd.edu";

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
  if(bpType == GSHARE){
    ghistory = 0;
    gshare_table = (uint8_t*)malloc((1<<ghistoryBits)*sizeof(uint8_t));
    for(int i = 0; i < (1<<ghistoryBits); i++){
      gshare_table[i] = WN;
    }
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
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{ 
  uint32_t gs_index,gs_mask;
  switch(bpType){
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
  }
}
