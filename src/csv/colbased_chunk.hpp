/*
    ParaText: parallel text reading
    Copyright (C) 2016. wise.io, Inc.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
  Coder: Damian Eads.
 */

#ifndef PARATEXT_COLBASED_CHUNK_HPP
#define PARATEXT_COLBASED_CHUNK_HPP

#include "generic/parse_params.hpp"
#include "util/widening_vector.hpp"
#include "util/strings.hpp"

#include <typeindex>
#include <sstream>

namespace ParaText {

namespace CSV {

  class ColHolder {
  public:
    ColHolder() {}
    virtual ~ColHolder() {}

    virtual ColHolder *process_categorical(const std::vector<char> &token) = 0;
    virtual ColHolder *process_float(float val) = 0;
    virtual ColHolder *process_integer(long val) = 0;

  private:
    std::vector<char> buffer_;
  };

  template <class ForceType, bool ForceNumeric>
  class NumColHolder {
  public:
    NumHolder() {}
    virtual ~NumColHolder() {}

    virtual ColHolder *process_categorical(const std::vector<char> &token) {
      process_categorical_impl<ForceNumeric>(token);
      return this;
    }

    template <bool ForceNumeric_>
    std::enable_if<!ForceNumeric_ && std::is_floating_point<ForceType>::value, ColHolder*> process_categorical_impl(const std::vector<char> &token) {
      std::unique_ptr<TextHolder> holder;
      for (size_t i = 0; i < data_.size(); i++) {
        Holder *other = holder->process_float(data_[i]);
        if (other == holder) {
          holder.reset(other);
        }
      }
      holder->process_categorical_impl(token);
      if (other == holder) {
        holder.reset(other);
      }
      return holder.release();
    }

    template <bool ForceNumeric_>
    std::enable_if<!ForceNumeric_ && std::is_integral<ForceType>::value, ColHolder*> process_categorical_impl(const std::vector<char> &token) {
      std::unique_ptr<TextHolder> holder;
      for (size_t i = 0; i < data_.size(); i++) {
        Holder *other = holder->process_float(data_[i]);
        if (other == holder) {
          holder.reset(other);
        }
      }
      holder->process_categorical_impl(token);
      if (other == holder) {
        holder.reset(other);
      }
      return holder.release();
    }

    template <bool ForceNumeric_>
    std::enable_if<ForceNumeric_, ColHolder*> process_categorical_impl(const std::vector<char> &token) {
      data_.push_back(parse<ForceType>(token.begin(), token.end()));
      return this;
    }

    virtual ColHolder *process_float(float val) {
      data_.push_back((ForceType)val);
      return this;
    };

    virtual ColHolder *process_integer(long val) {
      data_.push_back((ForceType)val);
      return this;
    }

  private:
    std::vector <ForceType> data_;
  };

  template <bool ForceNumeric>
  class NumColHolder<void, ForceNumeric> {
  public:
    NumHolder() {}
    virtual ~NumColHolder() {}

    virtual ColHolder *process_categorical(const std::vector<char> &token) {
      data_.push_back(parse<ForceType>(token_.begin(), token.end()));
      return this;
    }

    virtual ColHolder *process_float(float val) {
      data_.push_back(val);
      return this;
    };

    virtual ColHolder *process_integer(long val) {
      data_.push_back(val);
      return this;
    }

    template <bool ForceNumeric_>
    std::enable_if<!ForceNumeric_, ColHolder*> process_categorical_impl(const std::vector<char> &token) {
      std::unique_ptr<TextHolder> holder;
      std::type_index idx = data_.get_type_index();
      if (idx == std::type_index(id(float))) {
        for (size_t i = 0; i < data_.size(); i++) {
          Holder *other = holder->process_categorical(std::to_string(data_.get<float>[i]));
          if (other == holder) {
            holder.reset(other); 
          }
        }
      }
      holder->process_categorical_impl(token);
      if (other == holder) {
        holder.reset(other);
      }
      return holder.release();
    }

  private:
    widening_vector_dynamic<uint8_t, int8_t, int16_t, int32_t, int64_t, float> data_;
  };

  class TextColHolder {
  public:
    TextColHolder() {}
    virtual ~TextColHolder() {}

    virtual ColHolder *process_categorical(const std::vector<char> &token) {
      data_.emplace_back(token_.begin(), token.end());
      return this;
    }

    virtual ColHolder *process_float(float val) {
      data_.push_back(std::move(std::to_string(val)));
      return this;
    };

    virtual ColHolder *process_integer(long val) {
      data_.push_back(std::move(std::to_string(val)));
      return this;
    }

  private:
    std::vector<std::string> data_;
  };

  template <bool ForceCat, bool LevelLimit, bool LengthLimit>
  class CatColHolder {
  public:
    CatColHolder() {}
    virtual ~CatColHolder() {}

    virtual ColHolder *process_categorical(const std::vector<char> &token) {
      data_.push_back(get_level(token.begin(), token.end()));
      return this;
    }

    typename std::enable_if<!LevelLimit && LengthLimit, ColHolder *>::type process_categorical_impl(const std::vector<char> &token) {
      if (token.size() > length_limit) {
        
      }
    }

    virtual ColHolder *process_float(float val) {
      data_.push_back(get_level(std::to_string(val)));
      return this;
    };

    virtual ColHolder *process_integer(long val) {
      data_.push_back(get_level(std::to_string(val)));
      return this;
    }

  private:
    std::vector<std::string> data_;
  };

  /*
    Represents a chunk of parsed column data for a col-based CSV parser.
  */
  class ColBasedChunk {
  public:
    /*
      Creates a new chunk with an empty name.
     */
    ColBasedChunk() : max_level_name_length_(std::numeric_limits<size_t>::max()), max_levels_(std::numeric_limits<size_t>::max()), forced_semantics_(Semantics::UNKNOWN) {}

    /*
      Creates a new chunk.

      \param column_name      The name of the column for the chunk.
     */
    ColBasedChunk(const std::string &column_name)
      : column_name_(column_name), max_level_name_length_(std::numeric_limits<size_t>::max()), max_levels_(std::numeric_limits<size_t>::max()), forced_semantics_(Semantics::UNKNOWN) {}

    /*
      Creates a new chunk.

      \param column_name             The name of the column for the chunk.
      \param max_level_name_length   If this field length is exceeded, all string fields in a
                                     column are considered text rather than categorical levels.
      \param max_levels              If this number of levels is exceeded, then all string fields
                                     in a column are considered categorical.
     */
    ColBasedChunk(const std::string &column_name, size_t max_level_name_length, size_t max_levels, Semantics forced_semantics_)
      : column_name_(column_name), max_level_name_length_(max_level_name_length), max_levels_(max_levels), forced_semantics_(forced_semantics_) {}


    /*
     * Destroys this chunk.
     */
    virtual ~ColBasedChunk()             {}

    /*
     * Passes a floating point datum to the column handler. If categorical
     * data was previously passed to this handler, this datum will be converted
     * to a string and treated as categorical.
     */
    void process_float(float val)               {
      if (cat_data_.size() > 0 || forced_semantics_ == Semantics::CATEGORICAL || forced_semantics_ == Semantics::TEXT) {
        std::string s(std::to_string(val));
        process_categorical(s.begin(), s.end());
      }
      else {
        number_data_.push_back(val);
      }
    }

    /*
     * Passes a floating point datum to the column handler. If categorical
     * data was previously passed to this handler, this datum will be converted
     * to a string and treated as categorical.
     */
    void process_integer(long val)               {
      if (cat_data_.size() > 0 || forced_semantics_ == Semantics::CATEGORICAL || forced_semantics_ == Semantics::TEXT) {
        std::string s(std::to_string(val));
        process_categorical(s.begin(), s.end());
      }
      else {
        number_data_.push_back(val);
      }
    }

    /*
     * Passes a categorical datum to the column handler. If numerical data
     * was previously passed to this handler, all previous data passed will
     * be converted to a string.
     */
    template <class Iterator>
    void process_categorical(Iterator begin, Iterator end) {
      if (forced_semantics_ == Semantics::NUMERIC) {
        number_data_.push_back((float)bsd_strtod(begin, end));
      }
      else if (number_data_.size() > 0) {
        if (begin == end) {
          //std::cout << "{" << std::string(begin, end);
          number_data_.push_back((long)0);
        }
        else {
          //std::cout << "[" << std::string(begin, end);
          convert_to_cat_or_text();
          std::string key(begin, end);
          add_cat_data(key);
        }
      }
      else {
        std::string key(begin, end);
        add_cat_data(key);
      }
    }

    /*
      Returns the semantics of this column.
     */
    Semantics get_semantics() const {
      if (cat_data_.size() > 0) {
        return Semantics::CATEGORICAL;
      }
      else if (text_data_.size() > 0) {
        return Semantics::TEXT;
      }
      else {
        return Semantics::NUMERIC;
      }
    }

    /*
      Returns the type index of the data in this column.
     */
    std::type_index get_type_index() const {
      if (cat_data_.size() > 0) {
        return cat_data_.get_type_index();
      } else if (text_data_.size() > 0) {
        return std::type_index(typeid(text_data_));
      }
      else {
        return number_data_.get_type_index();
      }
    }

    std::type_index get_common_type_index(std::type_index &other) const {
      if (cat_data_.size() > 0 || other == std::type_index(typeid(std::string))) {
        return std::type_index(typeid(std::string));
      }
      else {
        return number_data_.get_common_type_index(other);
      }
    }

    template <class T>
    size_t insert_numeric(T *oit) {
      if (cat_data_.size() > 0) {
        throw std::logic_error("expected numeric data");
      }
      size_t to_copy = number_data_.size();
      for (size_t i = 0; i < to_copy; i++) {
        oit[i] = (T)number_data_.get<float>(i);
      }
      return to_copy;
    }

    template <class T, bool Numeric>
    inline typename std::enable_if<std::is_arithmetic<T>::value && Numeric, T>::type get(size_t i) const {
      return number_data_.get<T>(i);
    }

    template <class T, bool Numeric>
    inline typename std::enable_if<std::is_arithmetic<T>::value && !Numeric, T>::type get(size_t i) const {
      return cat_data_.get<size_t>(i);
    }

    const std::vector<std::string> &get_cat_keys() const {
      return cat_keys_;
    }

    size_t size() const {
      if (cat_data_.size() > 0) {
        return cat_data_.size();
      }
      else if (number_data_.size() > 0) {
        return number_data_.size();
      }
      else {
        return text_data_.size();
      }
    }

    void clear() {
      number_data_.clear();
      cat_data_.clear();
      cat_ids_.clear();
      cat_keys_.clear();
    }

    size_t get_string(size_t idx) {
      return cat_data_.get<size_t>(idx);
    }

    size_t get_string_id(const std::string &key) {
      auto it = cat_ids_.find(key);
      if (it == cat_ids_.end()) {
        std::tie(it, std::ignore) = cat_ids_.insert(std::make_pair(key, cat_ids_.size()));
        cat_keys_.push_back(key);
      }
      return it->second;
    }

    /*
     * Converts all floating point data collected by this handler into
     * categorical data.
     */
    void convert_to_cat_or_text() {
      if (number_data_.size() > 0) {
        for (size_t i = 0; i < number_data_.size(); i++) {
          add_cat_data(std::to_string(number_data_.get<float>(i)));
        }
        number_data_.clear();
        number_data_.shrink_to_fit();
      }
    }

    void convert_to_text() {
      if (number_data_.size() > 0 || forced_semantics_ == Semantics::TEXT) {
        for (size_t i = 0; i < number_data_.size(); i++) {
          text_data_.push_back(std::to_string(number_data_.get<float>(i)));
        }
        number_data_.clear();
        number_data_.shrink_to_fit();
      }
      else if (cat_data_.size() > 0) {
        for (size_t i = 0; i < cat_data_.size(); i++) {
          text_data_.push_back(cat_keys_[cat_data_.get<long>(i)]);
        }
        cat_data_.clear();
        cat_data_.shrink_to_fit();
        cat_ids_.clear();
        cat_keys_.clear();
        cat_keys_.shrink_to_fit();
      }
    }

    void add_cat_data(const std::string &data) {
      if (forced_semantics_ == Semantics::TEXT || text_data_.size() > 0) {
        text_data_.push_back(data);
      }
      else if (forced_semantics_ == Semantics::CATEGORICAL) {
        cat_data_.push_back((long)get_string_id(data));
      }
      else if (data.size() > max_level_name_length_ || cat_keys_.size() > max_levels_) {
        convert_to_text();
        text_data_.push_back(data);
      }
      else {
        cat_data_.push_back((long)get_string_id(data));
      }
    }

    const std::string &get_text(size_t i) const {
      return text_data_[i];
    }

  private:
    std::string column_name_;
    widening_vector_dynamic<uint8_t, int8_t, int16_t, int32_t, int64_t, float> number_data_;
    widening_vector_dynamic<uint8_t, uint8_t, uint16_t, uint32_t, uint64_t>    cat_data_;
    std::unordered_map<std::string, size_t>                                    cat_ids_;
    std::vector<std::string>                                                   cat_keys_;
    std::vector<std::string>                                                   text_data_;
    size_t                                                                     max_level_name_length_;
    size_t                                                                     max_levels_;
    Semantics                                                                  forced_semantics_;
  };
}
}
#endif