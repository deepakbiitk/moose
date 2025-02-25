//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MooseUtils.h"
#include "libmesh/communicator.h"

namespace StochasticTools
{

/**
 * Custom type trait that has a ::value of true for types that can be gathered
 */
template <typename T>
struct canDefaultGather
{
  static constexpr bool value = false;
};
template <typename T>
struct canDefaultGather<std::vector<T>>
{
  static constexpr bool value = std::is_base_of<TIMPI::DataType, TIMPI::StandardType<T>>::value;
};
template <typename T>
struct canStochasticGather
{
  static constexpr bool value = false;
};
template <typename T>
struct canStochasticGather<std::vector<T>>
{
  static constexpr bool value = canStochasticGather<T>::value ||
                                std::is_base_of<TIMPI::DataType, TIMPI::StandardType<T>>::value ||
                                std::is_same<T, std::string>::value || std::is_same<T, bool>::value;
};

/*
 * Methods for gathering nested vectors
 */
template <typename T>
void
stochasticGather(const libMesh::Parallel::Communicator &, processor_id_type, T &)
{
  ::mooseError("Cannot gather values of type ", MooseUtils::prettyCppType<T>());
}
template <typename T,
          typename std::enable_if<canDefaultGather<std::vector<T>>::value, int>::type = 0>
void
stochasticGather(const libMesh::Parallel::Communicator & comm,
                 processor_id_type root_id,
                 std::vector<T> & val)
{
  comm.gather(root_id, val);
}
template <
    typename T,
    typename std::enable_if<canStochasticGather<std::vector<std::vector<T>>>::value, int>::type = 0>
void
stochasticGather(const libMesh::Parallel::Communicator & comm,
                 processor_id_type root_id,
                 std::vector<std::vector<T>> & val)
{
  // Get local vector sizes
  std::size_t num_local_vecs = val.size();
  std::vector<std::size_t> val_sizes;
  val_sizes.reserve(num_local_vecs);
  std::size_t num_local_vals = 0;
  for (const auto & v : val)
  {
    val_sizes.push_back(v.size());
    num_local_vals += v.size();
  }

  // Flatten the local vector of vectors
  std::vector<T> val_exp;
  val_exp.reserve(num_local_vals);
  for (auto & v : val)
    std::copy(v.begin(), v.end(), std::back_inserter(val_exp));

  // Gather the vector sizes and the flattened vector
  comm.gather(root_id, val_sizes);
  stochasticGather(comm, root_id, val_exp);

  // Build the vector of vectors from the gathered flatten vector
  if (comm.rank() == root_id)
  {
    val.resize(val_sizes.size());
    std::size_t ind = num_local_vals;
    for (std::size_t i = num_local_vecs; i < val_sizes.size(); ++i)
    {
      val[i].resize(val_sizes[i]);
      std::move(val_exp.begin() + ind, val_exp.begin() + ind + val_sizes[i], val[i].begin());
      ind += val_sizes[i];
    }
  }
}
// Gathering a vector of strings hasn't been implemented in libMesh, so just gonna do it the hard
// way
template <typename T>
void
stochasticGather(const libMesh::Parallel::Communicator & comm,
                 processor_id_type root_id,
                 std::vector<std::basic_string<T>> & val)
{
  std::vector<std::basic_string<T>> val_gath = val;
  comm.allgather(val_gath);
  if (comm.rank() == root_id)
    val = std::move(val_gath);
}
// Gathering bool is weird
template <typename A>
void
stochasticGather(const libMesh::Parallel::Communicator & comm,
                 processor_id_type root_id,
                 std::vector<bool, A> & val)
{
  std::vector<unsigned short int> temp(val.size());
  for (std::size_t i = 0; i < val.size(); ++i)
    temp[i] = val[i] ? 1 : 0;
  comm.gather(root_id, temp);
  if (comm.rank() == root_id)
  {
    val.resize(temp.size());
    for (std::size_t i = 0; i < temp.size(); ++i)
      val[i] = temp[i] == 1;
  }
}

/*
 * Methods for gathering nested vectors on all processors
 */
template <typename T>
void
stochasticAllGather(const libMesh::Parallel::Communicator &, T &)
{
  ::mooseError("Cannot gather values of type ", MooseUtils::prettyCppType<T>());
}
template <typename T,
          typename std::enable_if<canDefaultGather<std::vector<T>>::value, int>::type = 0>
void
stochasticAllGather(const libMesh::Parallel::Communicator & comm, std::vector<T> & val)
{
  comm.allgather(val);
}
template <
    typename T,
    typename std::enable_if<canStochasticGather<std::vector<std::vector<T>>>::value, int>::type = 0>
void
stochasticAllGather(const libMesh::Parallel::Communicator & comm, std::vector<std::vector<T>> & val)
{
  // Get local vector sizes
  std::size_t num_local_vecs = val.size();
  std::vector<std::size_t> val_sizes;
  val_sizes.reserve(num_local_vecs);
  std::size_t num_local_vals = 0;
  for (const auto & v : val)
  {
    val_sizes.push_back(v.size());
    num_local_vals += v.size();
  }

  // Flatten the local vector of vectors
  std::vector<T> val_exp;
  val_exp.reserve(num_local_vals);
  for (auto & v : val)
    std::copy(v.begin(), v.end(), std::back_inserter(val_exp));

  // Gather the vector sizes and the flattened vector
  comm.allgather(val_sizes);
  stochasticAllGather(comm, val_exp);

  // Build the vector of vectors from the gathered flatten vector
  val.resize(val_sizes.size());
  std::size_t ind = 0;
  for (std::size_t i = 0; i < val_sizes.size(); ++i)
  {
    val[i].resize(val_sizes[i]);
    std::move(val_exp.begin() + ind, val_exp.begin() + ind + val_sizes[i], val[i].begin());
    ind += val_sizes[i];
  }
}
// Gathering a vector of strings hasn't been implemented in libMesh, so just gonna do it the hard
// way
template <typename T>
void
stochasticAllGather(const libMesh::Parallel::Communicator & comm,
                    std::vector<std::basic_string<T>> & val)
{
  comm.allgather(val);
}
// Gathering bool is weird
template <typename A>
void
stochasticAllGather(const libMesh::Parallel::Communicator & comm, std::vector<bool, A> & val)
{
  std::vector<unsigned short int> temp(val.size());
  for (std::size_t i = 0; i < val.size(); ++i)
    temp[i] = val[i] ? 1 : 0;
  comm.allgather(temp);
  val.resize(temp.size());
  for (std::size_t i = 0; i < temp.size(); ++i)
    val[i] = temp[i] == 1;
}

} // StochasticTools namespace
