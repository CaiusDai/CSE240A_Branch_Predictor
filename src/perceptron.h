//------------------------------------//
//    Predictor Function Prototypes   //
//------------------------------------//

// Initialize the predictor
//
void init_perceptron();

uint8_t perceptron_predict(uint32_t pc);

void perceptron_train_predictor(uint32_t pc, uint8_t outcome);
