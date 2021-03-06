/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

//Creating a random number generator
default_random_engine random_gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).
	
	// Set the number of articles. (May be it could be calculated by GPS coords, the position error and dentisiy of particles.)
	num_particles = 50;
	
	// Create normal distribution to be used for paritcles creating around GPS coords.
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);
	
	// Create particles
	for (int i = 0; i < num_particles; i++) {
		Particle particle;
		particle.id = i;
		particle.x = dist_x(random_gen);
		particle.y = dist_y(random_gen);
		particle.theta = dist_theta(random_gen);
		particle.weight	= 1.0;

		particles.push_back(particle);
		weights.push_back(particle.weight);
	}

	// We are all done initializing
	is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/
	
	// Loop over particles
    for (int i = 0; i < num_particles; i++) {
		// Get particle
		Particle &particle = particles[i];
        double pred_x, pred_y, pred_theta;

        // Update particle position and yaw rate
        if (yaw_rate == 0) { // yaw rate zero
            pred_x = particle.x + velocity * delta_t * cos(particle.theta);
            pred_y = particle.y + velocity * delta_t * sin(particle.theta);
            pred_theta = particle.theta;
        } else {
            pred_x = particle.x + velocity / yaw_rate * (sin(particle.theta + yaw_rate * delta_t) - sin(particle.theta));
            pred_y = particle.y + velocity / yaw_rate * (cos(particle.theta) - cos(particle.theta + yaw_rate * delta_t));
            pred_theta = particle.theta + yaw_rate * delta_t;
        }

        // Add noise
        normal_distribution<double> dist_x(pred_x, std_pos[0]);
        normal_distribution<double> dist_y(pred_y, std_pos[1]);
        normal_distribution<double> dist_theta(pred_theta, std_pos[2]);

        particle.x = dist_x(random_gen);
        particle.y = dist_y(random_gen);
        particle.theta = dist_theta(random_gen);
    }

}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.
	
	for (int i = 0; i < observations.size(); i++) {
		double min_dist = std::numeric_limits<double>::max();
		int min_index = -1;
		int id = 0;
		LandmarkObs &observed = observations[i];
		for (int l = 0; l < predicted.size(); l++) {
			LandmarkObs pred = predicted[l];
			double c_dist = dist(pred.x, pred.y, observed.x, observed.y);
			if (c_dist < min_dist) {
				min_dist = c_dist;
				min_index = l;
				id = pred.id;
			}
		}

		observed.idx = min_index;
		observed.id = id;
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html
	double sig_x = std_landmark[0];
	double sig_y = std_landmark[1];
	double gauss_norm = (1 / (2 * M_PI * sig_x * sig_y));
	double weights_sum = 0.0;
		
	for (int i = 0; i < num_particles; i++) {
		Particle &particle = particles[i];

		std::vector<int> associations;
        std::vector<double> sense_x;
        std::vector<double> sense_y;

		double sin_theta = sin(particles[i].theta);
		double cos_theta = cos(particles[i].theta);
        
		// Transform observation cordinates
		std::vector<LandmarkObs> trans_observations;
		for (int j = 0; j < observations.size(); j++) {
			LandmarkObs trans_obs;
			double tx = particle.x + observations[j].x * cos_theta - observations[j].y * sin_theta;
			double ty = particle.y + observations[j].x * sin_theta + observations[j].y * cos_theta;
			
			trans_obs.x = tx;
			trans_obs.y = ty;
			trans_obs.id = observations[j].id;

			trans_observations.push_back(trans_obs);
			sense_x.push_back(tx);
			sense_y.push_back(ty);
		}

		// get landmarks in range
		std::vector<LandmarkObs> predicted;

		for (int k = 0; k < map_landmarks.landmark_list.size(); k++) {
			Map::single_landmark_s c_landmark = map_landmarks.landmark_list[k];
			double c_dist = dist(particle.x, particle.y, c_landmark.x_f, c_landmark.y_f);
			if(c_dist <= sensor_range) {
				LandmarkObs lm;
				lm.x = c_landmark.x_f;
				lm.y = c_landmark.y_f;
				lm.id = c_landmark.id_i;
				predicted.push_back(lm);
			}
		}

        // Associate the nearest neighbor 
        dataAssociation(predicted, trans_observations);

        // Calculate new weight
        double weight = 1.0;
        for (int l = 0; l < trans_observations.size(); l++) {
			LandmarkObs c_obs = trans_observations[l];
			double x_obs = c_obs.x;
			double y_obs = c_obs.y;
			double mu_x = predicted[c_obs.idx].x;
			double mu_y = predicted[c_obs.idx].y;
			
			associations.push_back(c_obs.id);

			double exponent = (pow(x_obs - mu_x, 2) / (2.0 * pow(sig_x, 2))) + (pow(y_obs - mu_y, 2) / (2.0 * pow(sig_y, 2)));
			
			double prob = gauss_norm * exp(-exponent);

			if (prob > 0) {
				weight *= prob;
			}
		}
		
		particle.weight = weight;
		weights[i] = particle.weight;
		weights_sum += weight;
		
		SetAssociations(particle, associations, sense_x, sense_y);
	}

	// Normalize weights
	for (int i = 0; i < num_particles; i++) {
		particles[i].weight /= weights_sum;
		weights[i] = particles[i].weight;
	}

}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
	vector<Particle> resampled;
	discrete_distribution<int> d(weights.begin(), weights.end());
	
	for (int i = 0; i < num_particles; i++) {
		resampled.push_back(particles[d(random_gen)]);
	}
	particles = resampled;
	
}

void ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations = associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
