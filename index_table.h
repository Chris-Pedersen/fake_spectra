#ifndef INDEX_TABLE_H
#define INDEX_TABLE_H

#include <map>
#include <vector>
#include <valarray>
#include "types.h"

class IndexTable
{
public:
  IndexTable(const los *los_table_i, const int NumLos_i, const double box);

  //Get a list of lines nearby a particle with coordinates xx, yy, zz and smoothing length hh.
  std::map<int,double> get_near_lines(const float pos[], const double hh);

  //Find a list of particles near each line.
  //Output:
  //valarray, NumLos long.
  //Each element is a list of particles near that line,
  //and each list element is (particle index, distance from line).
  std::valarray< std::vector<std::pair <int, double> > > get_near_particles(const float pos[], const float hh[], const long long npart);

  //Get the axis of a line
  inline int get_axis(const int iproc)
  {
      return los_table[iproc].axis;
  }

private:
  //Get a list of lines nearby a particle from a particular index table
  void get_nearby(float first, std::multimap<const double, const int>& sort_los, std::map<int, double>& nearby, const float pos[], const double hh);
  //Get a list of lines nearby a particle from an iterator range
  void get_nearby_from_range(std::multimap<const double, const int>::iterator low, std::multimap<const double, const int>::iterator high, std::map<int, double>& nearby, const float pos[], const double hh);
  //Get the transverse distance from sightline iproc to position pos
  double calc_dr2(const int iproc, const float pos[]);

  // The key is the position of the primary axis, which is xx for index_table and yy for index_table_xx.
  // The value is the index of this entry in los_table.
  //index_table stores lines where axis = 2 or 3.
  std::multimap<const double, const int> index_table;
  //index_table_xx stores lines where axis = 1.
  std::multimap<const double, const int> index_table_xx;
  //Pointers to the original los table
  const los *los_table;
  const int NumLos;
  const double boxsize;
};

#endif
