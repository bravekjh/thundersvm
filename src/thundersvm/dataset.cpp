//
// Created by jiashuai on 17-9-17.
//
#include "thundersvm/dataset.h"
#include <omp.h>
#include <string>
using std::fstream;
using std::stringstream;

DataSet::DataSet() : total_count_(0), n_features_(0) {}

DataSet::DataSet(const DataSet::node2d &instances, int n_features, const vector<float_type> &y) :
        instances_(instances), n_features_(n_features), y_(y), total_count_(instances_.size()) {}
/*
void DataSet::load_from_file(string file_name) {
    struct timeval t1,t2;
    double timeuse;
    gettimeofday(&t1,NULL);
    y_.clear();
    instances_.clear();
    total_count_ = 0;
    n_features_ = 0;
    fstream file;
    file.open(file_name, fstream::in);
    CHECK(file.is_open()) << "file " << file_name << " not found";
    string line;

    while (getline(file, line)) {
        float_type y;
        int i;
        float_type v;
        stringstream ss(line);
        ss >> y;
        this->y_.push_back(y);
        this->instances_.emplace_back();//reserve space for an instance
        string tuple;
        while (ss >> tuple) {
            CHECK_EQ(sscanf(tuple.c_str(), "%d:%f", &i, &v), 2) << "read error, using [index]:[value] format";
            this->instances_[total_count_].emplace_back(i, v);
            if (i > this->n_features_) this->n_features_ = i;
        };
        total_count_++;
    }
    file.close();
    gettimeofday(&t2,NULL);
    timeuse = (t2.tv_sec - t1.tv_sec) * 1000.0 + (t2.tv_usec - t1.tv_usec)/1000.0;
    cout<<"Use Time:"<<timeuse<<endl;
}
*/
inline char* findlastline(char *ptr, char *begin) {
    for (; ptr != begin; --ptr) {
      if (*ptr == '\n' || *ptr == '\r') return ptr;
    }
    return begin;
}
void DataSet::load_from_file(string file_name){
    y_.clear();
    instances_.clear();
    total_count_ = 0;
    n_features_ = 0;
    std::ifstream ifs (file_name, std::ifstream::binary);
    CHECK(ifs.is_open()) << "file " << file_name << " not found";
    // get pointer to associated buffer object
    std::filebuf* pbuf = ifs.rdbuf();
    // get file size using buffer's members
    std::size_t fsize = pbuf->pubseekoff (0,ifs.end,ifs.in);
    pbuf->pubseekpos (0,ifs.in);
    // allocate memory to contain file data
    char* buffer=new char[fsize];
    // get file data
    pbuf->sgetn (buffer,fsize);
    ifs.close();
    char *head = buffer;
    const int nthread = omp_get_max_threads();
	vector<float_type> *y_thread = new vector<float_type>[nthread];
	vector<vector<DataSet::node>> *instances_thread = new vector<vector<DataSet::node>>[nthread];
	int *local_count = new int[nthread];
	int *local_feature = new int[nthread];
    memset(local_count, 0, nthread * sizeof(int));
    memset(local_feature, 0, nthread * sizeof(int));
    #pragma omp parallel num_threads(nthread)
    {
        int tid = omp_get_thread_num();
        size_t nstep = (fsize + nthread - 1) / nthread;
        size_t sbegin = min(tid * nstep, fsize);
        size_t send = min((tid + 1) * nstep, fsize);
        char *pbegin = findlastline(head + sbegin, head);
        char *pend;
        if (tid + 1 == nthread) {
          pend = head + send;
        } else {
          pend = findlastline(head + send, head);
        }
        char * lbegin;
        if(tid == 0) 
            lbegin = pbegin;
        else
            lbegin = pbegin + 1;
        char * lend = lbegin;
        while(1){
            lend = lbegin;
            if(lend == pend)
                break;
            if(*lend == '\n'){
                lbegin ++;
                continue;
            }
            while (lend != pend && *lend != '\n' && *lend != '\r'){
                ++lend;
            }
            string line(lbegin, lend);
            float_type y;
            int i;
            float_type v;
            stringstream ss(line);
            ss >> y;
            y_thread[tid].push_back(y);
            instances_thread[tid].emplace_back();//reserve space for an instance
            string tuple;
            while (ss >> tuple) {
                CHECK_EQ(sscanf(tuple.c_str(), "%d:%f", &i, &v), 2) << "read error, using [index]:[value] format";
                instances_thread[tid][local_count[tid]].emplace_back(i, v);
                if (i > local_feature[tid]) local_feature[tid] = i;
            };
            local_count[tid]++;
            if(lend == pend)
                break;
            lbegin = lend + 1;
        }
    }
    for(int i = 0; i < nthread; i++){
        if(local_feature[i] > n_features_)
            n_features_ = local_feature[i];
        total_count_ += local_count[i];
    }
    for(int i = 0; i < nthread; i++){
        this->y_.insert(y_.end(), y_thread[i].begin(), y_thread[i].end());
        this->instances_.insert(instances_.end(), instances_thread[i].begin(), instances_thread[i].end());
    }
}
/*
inline bool isdigitchars(char c) {
  return (c >= '0' && c <= '9')
    || c == '+' || c == '-'
    || c == '.'
    || c == 'e' || c == 'E';
}

inline bool isblank(char c) {
  return (c == ' ' || c == '\t');
}

inline int get_label(const char * begin, const char * end,
                     const char ** endptr, float &v1) { // NOLINT(*)
  const char * p = begin;
  while (p != end && !isdigitchars(*p)) ++p;
  if (p == end) {
    *endptr = end;
    return 0;
  }
  const char * q = p;
  while (q != end && isdigitchars(*q)) ++q;
  v1 = strtol(p, q);
  p = q;
  while (p != end && isblank(*p)) ++p;
  if (p == end || *p != ':') {
    // only v1
    *endptr = p;
    return 1;
  }
  else{
    return -1;
  }
}

inline int ParsePair(const char * begin, const char * end,
                     const char ** endptr, int &v1, float &v2) { // NOLINT(*)
  const char * p = begin;
  while (p != end && !isdigitchars(*p)) ++p;
  if (p == end) {
    *endptr = end;
    return 0;
  }
  const char * q = p;
  while (q != end && isdigitchars(*q)) ++q;
  v1 = strtol(p, q);
  p = q;
  while (p != end && isblank(*p)) ++p;
  if (p == end || *p != ':') {
    // only v1
    *endptr = p;
    return 1;
  }
  p++;
  while (p != end && !isdigitchars(*p)) ++p;
  q = p;
  while (q != end && isdigitchars(*q)) ++q;
  *endptr = q;
  v2 = strtof(p, q);
  return 2;
}

*/

void DataSet::load_from_python(float *y, char **x, int len){
    y_.clear();
    instances_.clear();
    total_count_ = 0;
    n_features_ = 0;
    for(int i = 0; i < len; i++){
        int ind;
        float_type v;
        string line = x[i];
        stringstream ss(line);
        y_.push_back(y[i]);
        instances_.emplace_back();
        string tuple;
        while(ss >> tuple){
            CHECK_EQ(sscanf(tuple.c_str(), "%d:%f", &ind, &v), 2) << "read error, using [index]:[value] format";
            instances_[total_count_].emplace_back(ind, v);
            if(ind > n_features_) n_features_ = ind;
        };
        total_count_++;
    }
}
const vector<int> &DataSet::count() const {//return the number of instances of each class
    return count_;
}

const vector<int> &DataSet::start() const {
    return start_;
}

size_t DataSet::n_classes() const {
    return start_.size();
}

const vector<int> &DataSet::label() const {
    return label_;
}

void DataSet::group_classes(bool classification) {
    if (classification) {
        start_.clear();
        count_.clear();
        label_.clear();
        perm_.clear();
        vector<int> dataLabel(y_.size());//temporary labels of all the instances

        //get the class labels; count the number of instances in each class.
        for (int i = 0; i < y_.size(); ++i) {
            int j;
            for (j = 0; j < label_.size(); ++j) {
                if (y_[i] == label_[j]) {
                    count_[j]++;
                    break;
                }
            }
            dataLabel[i] = j;
            //if the label is unseen, add it to label vector.
            if (j == label_.size()) {
                //real to int conversion is safe, because group_classes only used in classification
                label_.push_back(int(y_[i]));
                count_.push_back(1);
            }
        }

        //logically put instances of the same class consecutively.
        start_.push_back(0);
        for (int i = 1; i < count_.size(); ++i) {
            start_.push_back(start_[i - 1] + count_[i - 1]);
        }
        vector<int> start_copy(start_);
        perm_ = vector<int>(y_.size());//index of each instance in the original array
        for (int i = 0; i < y_.size(); ++i) {
            perm_[start_copy[dataLabel[i]]] = i;
            start_copy[dataLabel[i]]++;
        }
    } else {
        for (int i = 0; i < instances_.size(); ++i) {
            perm_.push_back(i);
        }
        start_.push_back(0);
        count_.push_back(instances_.size());
    }
}

size_t DataSet::n_instances() const {//return the total number of instances
    return total_count_;
}

size_t DataSet::n_features() const {
    return n_features_;
}

const DataSet::node2d &DataSet::instances() const {//return all the instances
    return instances_;
}

const DataSet::node2d DataSet::instances(int y_i) const {//return instances of a given class
    int si = start_[y_i];
    int ci = count_[y_i];
    node2d one_class_ins;
    for (int i = si; i < si + ci; ++i) {
        one_class_ins.push_back(instances_[perm_[i]]);
    }
    return one_class_ins;
}

const DataSet::node2d DataSet::instances(int y_i, int y_j) const {//return instances of two classes
    node2d two_class_ins;
    node2d i_ins = instances(y_i);
    node2d j_ins = instances(y_j);
    two_class_ins.insert(two_class_ins.end(), i_ins.begin(), i_ins.end());
    two_class_ins.insert(two_class_ins.end(), j_ins.begin(), j_ins.end());
    return two_class_ins;
}

const vector<int> DataSet::original_index() const {//index of each instance in the original array
    return perm_;
}

const vector<int> DataSet::original_index(int y_i) const {//index of each instance in the original array for one class
    return vector<int>(perm_.begin() + start_[y_i], perm_.begin() + start_[y_i] + count_[y_i]);
}

const vector<int>
DataSet::original_index(int y_i, int y_j) const {//index of each instance in the original array for two class
    vector<int> two_class_idx;
    vector<int> i_idx = original_index(y_i);
    vector<int> j_idx = original_index(y_j);
    two_class_idx.insert(two_class_idx.end(), i_idx.begin(), i_idx.end());
    two_class_idx.insert(two_class_idx.end(), j_idx.begin(), j_idx.end());
    return two_class_idx;
}

const vector<float_type> &DataSet::y() const {
    return y_;
}


