// partition.h

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: johans@google.com (Johan Schalkwyk)
//
// \file Functions and classes to create a partition of states
//

#ifndef FST_LIB_PARTITION_H__
#define FST_LIB_PARTITION_H__

#include <vector>
using std::vector;
#include <algorithm>


#include <fst/queue.h>



namespace fst {

template <typename T> class PartitionIterator;

// \class Partition
// \brief Defines a partitioning of 'elements'-  used to represent
//        equivalence classes for Fst operations like minimization.
//        Note: T must be a signed integer type.
//
//        The elements are numbered from 0 to num_elements - 1.
//        Initialize(num_elements) sets up the class for a given number of
//        elements.  We maintain a partition of these elements into
//        classes.  The classes are also numbered from zero; you can add a class
//        with AddClass(), or add them in bulk with
//        AllocateClasses(num_classes).  Initially the elements are not assiagned
//        to any class; you set up the initial mapping from elements to classes
//        by calling Add(element_id, class_id).  You can also move an element
//        to a different class by calling Move(element_id, class_id).
//
//        We also support a rather specialized interface that allows you to
//        efficiently split classes in the Hopcroft minimization algorihtm.
//        This maintains a binary partition of each class.  Let's call these,
//        rather arbitrarily, the 'yes' subset and the 'no' subset of each
//        class, and assume that by default, each element of a class is in its
//        'no' subset.  When you call SplitOn(element_id), you move element_id
//        to to the 'yes' subset of its class.  (if it was already in the 'yes'
//        set, it just stays there).  The aim is to enable you, later on, to
//        split the class in two in time no greater than the time you already
//        spent calling SplitOn() for that class.  We keep a list of the classes
//        which have nonempty 'yes' sets, as visited_classes_.  When you call
//        FinalizeSplit(Queue *l), for each class in visited_classes_ whose
//        'yes' and 'no' sets are both nonempty, it will create a new class
//        consisting of the smaller of the two subsets (and this class will be
//        added to the queue), and the old class will now be the larger of the
//        two subsets.  This call also resets all the yes/no partitions so that
//        everything is in the 'no' subsets.
//
//        Note: you can't use the Move() function if you have called SplitOn()
//        and haven't subsequently called FinalizeSplit().
template <typename T>
class Partition {
 public:
  Partition() { }

  Partition(T num_elements) {
    Initialize(num_elements);
  }

  // Create an empty partition for num_elements.  This means that the
  // elements are not assigned to a class (i.e class_index = -1);
  // you should set up the number of classes using AllocateClasses()
  // or AddClass(), and allocate each element to a class by
  // calling Add(element, class_id).
  void Initialize(size_t num_elements) {
    elements_.resize(num_elements);
    classes_.reserve(num_elements);
    classes_.clear();
    yes_counter_ = 1;
  }

  // Add a class; returns new number of classes.
  T AddClass() {
    size_t num_classes = classes_.size();
    classes_.resize(num_classes + 1);
    return num_classes;
  }

  // Adds 'num_classes' new (empty) aclasses.
  void AllocateClasses(T num_classes) {
    classes_.resize(classes_.size() + num_classes);
  }

  // Add element_id to class_id.  element_id should already have been allocated
  // by calling Initialize(num_elements) [or the constructor taking
  // num_elements] with num_elements > element_id.  element_id must not
  // currently be a member of any class; once elements have been added to
  // a class, use the Move() method to move them from one class to another.
  void Add(T element_id, T class_id) {
    Element &this_element = elements_[element_id];
    Class &this_class = classes_[class_id];
    this_class.size++;
    // add the element to the 'no' subset of the class.
    T no_head = this_class.no_head;
    if (no_head >= 0)
      elements_[no_head].prev_element = element_id;
    this_class.no_head = element_id;

    this_element.class_id = class_id;
    this_element.yes = 0;  // it's added to the 'no' subset of the class.
    this_element.next_element = no_head;
    this_element.prev_element = -1;
  }

  // Move element_id from 'no' subset of its current class to 'no' subset of
  // class class_id.  This may not work correctly if you have called SplitOn()
  // [for any element] and haven't subsequently called FinalizeSplit().
  void Move(T element_id, T class_id) {
    Element *elements = &(elements_[0]);
    Element &element = elements[element_id];
    CHECK(element.yes != yes_counter_);  // I'll remove this later.
    Class &old_class = classes_[element.class_id];
    old_class.size--;
    CHECK(old_class.size >= 0 && old_class.yes_size == 0);
    // excise the element from the 'no' list of its old class, where it is
    // assumed to be.
    if (element.prev_element >= 0) {
      elements[element.prev_element].next_element = element.next_element;
    } else {
      CHECK(old_class.no_head == element_id);
      old_class.no_head = element.next_element;
    }
    if (element.next_element >= 0)
      elements[element.next_element].prev_element = element.prev_element;
    // add to new class.
    Add(element_id, class_id);
  }

  // If 'element_id' was in the 'no' subset of its class, it is moved to the
  // 'yes' subset, and we make sure its class is included in the
  // 'visited_classes_' list.
  void SplitOn(T element_id) {
    Element *elements = &(elements_[0]);
    Element &element = elements[element_id];
    if (element.yes == yes_counter_) return;  // already in the 'yes' set; nothing to do.
    T class_id = element.class_id;
    Class &this_class = classes_[class_id];
    // excise the element from the 'no' list of its class.
    if (element.prev_element >= 0) {
      elements[element.prev_element].next_element = element.next_element;
    } else {
      CHECK(this_class.no_head == element_id);
      this_class.no_head = element.next_element;
    }
    if (element.next_element >= 0) {
      elements[element.next_element].prev_element = element.prev_element;
    }
    // add the element to the 'yes' list.
    if (this_class.yes_head >= 0) {
      elements[this_class.yes_head].prev_element = element_id;
    } else {
      visited_classes_.push_back(class_id);
    }
    element.yes = yes_counter_;
    element.next_element = this_class.yes_head;
    element.prev_element = -1;
    this_class.yes_head = element_id;
    this_class.yes_size++;
    CHECK(this_class.yes_size <= this_class.size);
  }

  // This function is to be called after you have possibly called SplitOn for
  // one or more elements [thus moving those elements to the 'yes' subset for
  // their class].  For each class that has a nontrivial split (i.e. it's not
  // the case that all members are in the 'yes' or 'no' subset), this function
  // creates a new class containing the smaller of the two subsets of elements,
  // leaving the larger group of elements in the old class.  The identifier of
  // the new class will be added to the queue provided as the pointer 'L'.  This
  // function then moves all elements to the 'no' subset of their class.
  template <class Queue>
  void FinalizeSplit(Queue* L) {
    for (size_t i = 0, size = visited_classes_.size(); i < size; ++i) {
      T new_class = SplitRefine(visited_classes_[i]);
      if (new_class != -1 && L)
        L->Enqueue(new_class);
    }
    visited_classes_.clear();
    // incrementing yes_counter_ is an efficient way to set all the 'yes'
    // members of the elements to false.
    yes_counter_++;
  }

  const T class_id(T element_id) const {
    return elements_[element_id].class_id;
  }

  const size_t class_size(T class_id)  const { return classes_[class_id].size; }

  const T num_classes() const { return classes_.size(); }

 private:
  friend class PartitionIterator<T>;

  // information about a given element
  struct Element {
    // this struct doesn't have a constructor; when the user calls Add() for
    // each element, we'll set all fields.
    T class_id;  // class-id of this element
    T yes;      // this is to be interpreted as a bool, true if it's in the
                // 'yes' set of this class.  the interpretation as bool is, (yes
                // == yes_counter_ ? true : false).
    T next_element;  // next element in the 'no' list or 'yes' list of this class,
                     // whichever of the two we belong to (think of this as the 'next'
                     // in a doubly-linked list, although it's an index into the
                     // elements array).  negative value corresponds to NULL.
    T prev_element;  // previous element in the 'no' or 'yes' doubly linked
                     // list.  negative value corresponds to NULL.
  };

  // information about a given class.
  struct Class {
    Class(): size(0), yes_size(0), no_head(-1), yes_head(-1) { }
    T size;  // total number of elements in this class ('no' plus 'yes' subsets)
    T yes_size;  // total number of elements of 'yes' subset of this class.
    T no_head;  // index of head element of doubly-linked list in 'no' subset.
                // [everything is in the 'no' subset until you call SplitOn()].
                // -1 means no element.
    T yes_head;  // index of head element of doubly-linked list in 'yes' subset.
                 // -1 means no element.
  };

  // This function, called from FinalizeSplit(), checks whether
  // a class has to be split (a class will be split only if its 'yes'
  // and 'no' subsets are both nonempty, but we can assume that since this
  // function was called, the 'yes' subset is nonempty).  It splits by
  // taking the smaller subset and making it a new class, and leaving
  // the larger subset of elements in the 'no' subset of the old class.
  // it returns the new class if created, or -1 if none was created.
  T SplitRefine(T class_id) {
    T yes_size = classes_[class_id].yes_size,
        size = classes_[class_id].size,
        no_size = size - yes_size;
    if (no_size == 0) {
      // all members are in the 'yes' subset -> don't have to create a new
      // class, just move them all to the 'no' subset.
      CHECK(classes_[class_id].no_head < 0);
      classes_[class_id].no_head = classes_[class_id].yes_head;
      classes_[class_id].yes_head = -1;
      classes_[class_id].yes_size = 0;
      return -1;
    } else {
      T new_class_id = classes_.size();
      classes_.resize(classes_.size() + 1);
      Class &old_class = classes_[class_id],
          &new_class = classes_[new_class_id];
      // note: new_class will have the values from the constructor.
      if (no_size < yes_size) {
        // move the 'no' subset to new class (as its 'no' subset).
        new_class.no_head = old_class.no_head;
        new_class.size = no_size;
        // and make the 'yes' subset of the old class, its 'no' subset
        old_class.no_head = old_class.yes_head;
        old_class.yes_head = -1;
        old_class.size = yes_size;
        old_class.yes_size = 0;
      } else {
        // move the 'yes' subset to the new class (as its 'no' subset)
        new_class.size = yes_size;
        new_class.no_head = old_class.yes_head;
        // retain only the 'no' subset in the old class.
        old_class.size = no_size;
        old_class.yes_size = 0;
        old_class.yes_head = -1;
      }
      Element *elements = &(elements_[0]);
      // update the 'class_id' of all the elements we moved.
      for (T e = new_class.no_head; e >= 0; e = elements[e].next_element)
        elements[e].class_id = new_class_id;
      return new_class_id;
    }
  }


  // elements_[i] contains all info about the i'th element
  vector<Element> elements_;

  // classes_[i] contains all info about the i'th class
  vector<Class> classes_;

  // set of visited classes to be used in split refine
  vector<T> visited_classes_;

  // yes_counter_ is used in interpreting the 'yes' members of class Element.
  // if element.yes == yes_counter_, we interpret that element as being in the
  // 'yes' subset of its class.  This allows us to, in effect, set all those
  // bools to false at a stroke by incrementing yes_counter_.
  T yes_counter_;


};


// iterate over members of the 'no' subset of a class in a partition.
// (when this is used, everything is in the 'no' subset).
template <typename T>
class PartitionIterator {
  typedef typename Partition<T>::Element Element;
 public:
  PartitionIterator(const Partition<T>& partition, T class_id)
      : p_(partition),
        element_id_(p_.classes_[class_id].no_head),
        class_id_(class_id) {}

  bool Done() {
    return (element_id_ < 0);
  }

  const T Value() {
    return element_id_;
  }

  void Next() {
    element_id_ = p_.elements_[element_id_].next_element;
  }

  void Reset() {
    element_id_ = p_.classes_[class_id_].no_head;
  }

 private:
  const Partition<T>& p_;
  T element_id_;
  T class_id_;
};
}  // namespace fst

#endif  // FST_LIB_PARTITION_H__
