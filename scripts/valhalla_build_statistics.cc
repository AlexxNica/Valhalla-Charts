#include "valhalla_build_statistics.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/format.hpp>
#include <sqlite3.h>

using dataPair = std::pair<std::vector<std::string>*, std::vector<std::vector<float>>*>;

int main (int argc, char** argv) {
  // If there is no input file print and error and exit
  if (argc < 2) {
    std::cout << "ERROR: No input file specified." << std::endl;
    std::cout << "Usage: ./sqlite2json statistics.sqlite" << std::endl;
    exit(0);
  }

  // data structures
  std::vector<std::string> classes;
  std::vector<std::string> countries;
  std::vector<std::vector<float>> data;

  // open DB file
  sqlite3 *db;
  int rc = sqlite3_open(argv[1], &db);
  if ( rc ) {
    std::cout << "Opening DB failed: " << sqlite3_errmsg(db);
    exit(0);
  }

  // fill data structures
  fillClasses(db, classes);
  fillCountryData(db, countries, data);
  
  generateJson(countries, data, classes);
  
  sqlite3_close(db);
  return 0;
}

/*
 * Handle errors returned from the db
 * Params
 *  rc - response code from previous sqlite3_exec call
 *  errMsg - char* passed into the previous sqlite3_exec call
 * Post
 *  returns normally if there is not error
 *  prints the db error and exits if an error occurred
 */
void checkDBResponse(int rc, char* errMsg) {
  if ( rc != SQLITE_OK ) {
    std::cout << "SQL error: " << errMsg << std::endl;
    sqlite3_free(errMsg);
    exit(0);
  } 
}

/*
 * Queries the database to get road class types
 * Params
 *  *db - sqlite3 database connection object
 *  classes - a vector of strings representing the road types
 *
 * Post
 *  The array containing the road classes is filled
 */
void fillClasses(sqlite3 *db, std::vector<std::string>& classes) {
  
  std::string sql = "SELECT * FROM countrydata LIMIT 1";
  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), classCallback, (void*) &classes, &errMsg);
  checkDBResponse(rc, errMsg);

}

/*
 * Queries the database to get length of roads in each class per country
 * Params
 *  *db - sqlite3 database connection object
 *  countries - vector of country iso codes
 *  data - 2D vector of road lengths
 *    each column belongs to a certain country
 *    each row is a type of road
 * Post
 *  countries contains the iso codes of all countries in the database
 *  data contains the lengths of each type of road for each country
 */
void fillCountryData(sqlite3 *db, std::vector<std::string>& countries, std::vector<std::vector<float>>& data) {
  
  std::string sql = "SELECT * FROM countrydata WHERE isocode IS NOT \"\"";

  char *errMsg = 0;
  dataPair data_pair = {&countries, &data};
  int rc = sqlite3_exec(db, sql.c_str(), countryDataCallback, (void*) &data_pair, &errMsg);
  checkDBResponse(rc, errMsg);
}

/*
 * Unused / Not-Implemented yet
 */
void fillMaxSpeedData(sqlite3 *db) {
  
  std::string sql = "SELECT isocode,type,maxspeed";
  sql += " FROM rclassctrydata";
  sql += " WHERE (type='Motorway' OR type='Trunk' OR type='Primary' OR type='Secondary')";
  sql += " AND isocode IS NOT \"\"";

  char *errMsg = 0;
  int rc = sqlite3_exec(db, sql.c_str(), maxSpeedCallback, NULL, &errMsg);
  checkDBResponse(rc, errMsg);
}

static int classCallback (void *data, int argc, char **argv, char **colName) {
  std::vector<std::string> *cat = (std::vector<std::string>*) data;
  for (int i = 1; i < argc; ++i) {
    cat->push_back(colName[i]);
  }
  cat->push_back("total");
  return 0;
}

/*
 * Callback function to reveive query results from sqlite3_exec
 * Params
 *  *dataPair - the pointer used to pass data structures into this function
 *  argc - # arguments
 *  **argv - char array of arguments
 *  **colName - char array returned column names from the database
 */
static int countryDataCallback (void *data_pair, int argc, char **argv, char **colName) {
  dataPair* p = (dataPair*) data_pair;
  auto* countries = std::get<0>(*p);
  auto* data = std::get<1>(*p);

  countries->push_back(argv[0]);
  std::string line = "";
  for (int i = 1; i < argc; ++i) {
    line += argv[i];
    line += " ";
  }

  std::istringstream iss(line);
  float tok;
  float sum = 0;
  std::vector<float> *classData = new std::vector<float>();

  while (iss >> tok) {
    classData->push_back(tok);
    sum += tok;
  }
  classData->push_back(sum);
  data->push_back(*classData);

  return 0;
}

/* UNUSED
 * Callback function to reveive query results from sqlite3_exec
 * Params
 *  *dataPair - the pointer used to pass data structures into this function
 *  argc - # arguments
 *  **argv - char array of arguments
 *  **colName - char array returned column names from the database
 */
static int maxSpeedCallback (void *data, int argc, char **argv, char **colName) {

  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << " ";
  }
  std::cout << std::endl;
  return 0;
}

/*
 * Generates a javascript file that has a function to return
 *  all the data queried from the database
 * Params
 *  countries - names of all the countries from the database
 *  data - float values for all the lengths of road per country
 *  classes - the types of road classes
 */
void generateJson (std::vector<std::string>& countries, std::vector<std::vector<float>>& data, std::vector<std::string>& classes) {

  std::ofstream out ("road_data.json");

  std::stringstream str;
  str << "{\n";
  std::string fmt = "\"%1%\" : {\n";
  for (size_t i = 0; i < countries.size(); ++i) {
    if (i) fmt = ",\n\"%1%\" : {\n";
    str << boost::format(fmt) % countries[i];
    str << boost::format("  \"name\" : \"%1%\",\n") % countries[i];
    str << "  \"records\": {\n";
    std::string fmt2 = "    \"%1%\": %2$.2f";
    for (size_t j = 0; j < classes.size(); ++j) {
      if (j) fmt2 = ",\n    \"%1%\": %2$.2f";
      str << boost::format(fmt2) % classes[j] % data[i][j];
    }
    str << "\n  }}";
  }
  str << "\n}";

  out << str.str() << std::endl;

  out.close();
}
