#include"Chromosome.h"
Chromosome::Chromosome() 
{
	fitness = 0;
	vehicle_assignment.clear();
	operation_sequence.clear();
	transport_sequence.clear();
}
Chromosome::~Chromosome() 
{
	vehicle_assignment.clear();
	operation_sequence.clear();
	transport_sequence.clear();
}
Chromosome::Chromosome(const Chromosome& other) 
{
	fitness = other.fitness;
	vehicle_assignment = other.vehicle_assignment;
	operation_sequence = other.operation_sequence;
	transport_sequence = other.transport_sequence;
}
Chromosome& Chromosome::operator=(const Chromosome& other) 
{
	if (this != &other) {
		fitness = other.fitness;
		vehicle_assignment = other.vehicle_assignment;
		operation_sequence = other.operation_sequence;
		transport_sequence = other.transport_sequence;
	}
	return *this;
}
void Chromosome::calculate() 
{
	int time = 0;

	fitness = time;
}
