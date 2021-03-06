#define _USE_MATH_DEFINES

#include <iostream>
#include <vector>
#include <cmath>

#include "individual.h"

const int popsize = 5000;
std::vector<Individual> pop(5000); // population size: 5000
//Varied Parameters
// A and B were varied between 0 and 1 (A+B = 1) to investigate deterministic vs stochastic env., not found to have a great effect 

float A = 1.f;   //Deterministic scaling constant, default value 
float B = 0.f;   //stochastic scaling constant, default value

//R between 1 and 100000, P between 0 and 1
float R = 10.f;  //Environmental variation
float P = 0.99f;   //predictability
///
const auto& env_dist = std::uniform_real_distribution<float>(-1.f, 1.f); // not explicitly stated in botero 2015
float E;
float Cue;

int gmax = 50000;
int tmax = 5;

//Mutation
const float mrate = 0.001f;
const float mmean = 0.f;
const float mshape = 0.05f;

//Costs
const float kd = 0.02f;
const float ka = 0.01f;
const float tau = 0.25;
//float q = 2.2;

//Cue range for reaction norm
const float cue_max = 1.f;
const float cue_min = 0.f;
const float cue_inc = 0.05f;

//R between 1 and 100000, P between 0 and 1
std::vector<float> vecR = { 1.f, powf(10.f, 0.5f), 10.f, powf(10.f, 1.5f), 100.f, powf(10.f, 2.5f), 1000.f, powf(10.f, 3.5f), 10000.f, powf(10.f, 4.5f), 100000.f };
std::vector<float> vecP = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };

// mutation prob
std::bernoulli_distribution mut_event(0.001); // mutation probability
// mutation size
std::cauchy_distribution<double> m_shift(0.0, 0.01); // how much of mutation

// reproduction
void reproduction(std::vector<Individual> & pop, float kd, float ka, float tau)
{
    // make fitness vec
    std::vector<double> fitness_vec;
    float max = 0.f; float min = 0.f;
    for (int a = 0; a < popsize; a++)
    {
        fitness_vec.push_back(pop[a].calculate_fitness(kd, ka, tau));
    }

    // make temp pop vector, position and energy vectors
    std::vector<Individual> tmp_pop(popsize);

    // assign parents
    for (int a = 0; a < popsize; a++) {

        std::discrete_distribution<> weighted_lottery(fitness_vec.begin(), fitness_vec.end());
        int parent_id = weighted_lottery(rnd::reng);

        // replicate ann_dev
        tmp_pop[a].ann_dev = pop[parent_id].ann_dev;
        tmp_pop[a].ann_life = pop[parent_id].ann_life;

        // reset mismatch
        tmp_pop[a].mismatch = 0.f;

        // mutate ann_dev
        for (auto& w : tmp_pop[a].ann_dev) {
            // probabilistic mutation of ANN
            if (mut_event(rnd::reng)) {
                w += static_cast<float> (m_shift(rnd::reng));
            }
        }

        // mutate ann_life
        for (auto& w : tmp_pop[a].ann_life) {
            // probabilistic mutation of ANN
            if (mut_event(rnd::reng)) {
                w += static_cast<float> (m_shift(rnd::reng));
            }
        }

    }

    //overwrite old pop
    std::swap(pop, tmp_pop);
    tmp_pop.clear();

}

// adjust pop size
void adjust_popsize(std::vector<Individual>& tmp_pop, const int targetsize) {

    while (static_cast<int>(tmp_pop.size()) < targetsize) {
        int duplicate = std::uniform_int_distribution<int>(0, tmp_pop.size() - 1)(rnd::reng);
        tmp_pop.push_back(tmp_pop[duplicate]);
    }

    while (static_cast<int>(tmp_pop.size()) > targetsize) {
        int remove = std::uniform_int_distribution<int>(0, tmp_pop.size() - 1)(rnd::reng);
        tmp_pop.erase(tmp_pop.begin() + remove);
    }

}

/// main function

int main() {

    // standard vector of cues
    std::vector<float> vec_cues;
    for (float i = cue_min; i < cue_max; ++cue_inc)
    {
        vec_cues.push_back(i);
    }

    for (int r = 0; r < vecR.size(); ++r) {

        float R = vecR[r];

        for (int p = 0; p < vecP.size(); ++p) {

            float P = vecP[p];

            //Initialization
            E = 0.f;
            if (1.f - P == 0.f) {
                Cue = E;
            }
            else {
                Cue = std::normal_distribution<float>(P * E, ((1.f - P) / 3.f))(rnd::reng);
            }

            for (int i = 0; i < static_cast<int>(pop.size()); i++) {
                pop[i].update_I_g(Cue);
            }

            // generations
            for (int g = 0; g < gmax; g++)
            {
                std::cout << "gen = " << g << "\n";

                for (int t = 0; t < tmax; t++) {
                    //update environment
                    E = A * std::sin((2 * M_PI * (g * tmax + t)) / (tmax * R)) + B * env_dist(rnd::reng);
                    //calculate cue
                    Cue = std::normal_distribution<float>(P * E, (1 - P) / 3)(rnd::reng);
                    /// Is Cue calculated once for the whole population, or per individual?
                    for (int i = 0; i < pop.size(); ++i) {

                        pop[i].update_I_g(Cue); // development cue

                    }
                    //individual update during lifetime
                    for (int i = 0; i < pop.size(); ++i) {

                        pop[i].update_I_t(Cue);
                        pop[i].update_mismatch(E);

                    }

                }
                reproduction(pop, kd, ka, tau);

            }


            const std::string outfile = "data_ann_logR" + std::to_string(log10f(R)).substr(0,3) + "_P" + std::to_string(P).substr(0,3) + ".txt";


            std::ofstream ofs(outfile);

            ofs << "ind" << "\t" << "cue" << "resp" << "\n";

            for (int i = 0; i < static_cast<int>(pop.size()); ++i) {

                std::vector<float> vec_resp = pop[i].get_reaction(vec_cues);

                for(int j = 0; j < static_cast<int>(vec_cues.size()); j++){

                    ofs << i << "\t" << vec_cues[j] << "\t" << vec_resp[j] << "\n";

                }



            }
            ofs.close();
        }




        return 0;
    }








