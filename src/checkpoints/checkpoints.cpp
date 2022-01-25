// Copyright (c) 2014-2020, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include <functional>
#include <vector>

using namespace epee;

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  /**
   * @brief struct for loading a checkpoint from json
   */
  struct t_hashline
  {
    uint64_t height; //!< the height of the checkpoint
    std::string hash; //!< the hash for the checkpoint
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(height)
          KV_SERIALIZE(hash)
        END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief struct for loading many checkpoints from json
   */
  struct t_hash_json {
    std::vector<t_hashline> hashlines; //!< the checkpoint lines from the file
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(hashlines)
        END_KV_SERIALIZE_MAP()
  };

  //---------------------------------------------------------------------------
  checkpoints::checkpoints()
  {
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str, const std::string& difficulty_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
    if (!difficulty_str.empty())
    {
      try
      {
        difficulty_type difficulty(difficulty_str);
        if (m_difficulty_points.count(height))
        {
          CHECK_AND_ASSERT_MES(difficulty == m_difficulty_points[height], false, "Difficulty checkpoint at given height already exists, and difficulty for new checkpoint was different!");
        }
        m_difficulty_points[height] = difficulty;
      }
      catch (...)
      {
        LOG_ERROR("Failed to parse difficulty checkpoint: " << difficulty_str);
        return false;
      }
    }
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    is_a_checkpoint = it != m_points.end();
    if(!is_a_checkpoint)
      return true;

    if(it->second == h)
    {
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
      return true;
    }else
    {
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH: " << it->second << ", FETCHED HASH: " << h);
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h) const
  {
    bool ignored;
    return check_block(height, h, ignored);
  }
  //---------------------------------------------------------------------------
  //FIXME: is this the desired behavior?
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if (0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    // Is blockchain_height before the first checkpoint?
    if (it == m_points.begin())
      return true;

    --it;
    uint64_t checkpoint_height = it->first;
    return checkpoint_height < block_height;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    if (m_points.empty())
      return 0;
    return m_points.rbegin()->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, difficulty_type>& checkpoints::get_difficulty_points() const
  {
    return m_difficulty_points;
  }

  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for (auto& pt : other.get_points())
    {
      if (m_points.count(pt.first))
      {
        CHECK_AND_ASSERT_MES(pt.second == m_points.at(pt.first), false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
      }
    }
    return true;
  }

  bool checkpoints::init_default_checkpoints(network_type nettype)
  {
    if (nettype == TESTNET)
    {
      ADD_CHECKPOINT2(0,     "48ca7cd3c8de5b6a4d53d2861fbdaedca141553559f9be9520068053cda8430b", "0x1");
      return true;
    }
    if (nettype == STAGENET)
    {
      ADD_CHECKPOINT2(0,       "76ee3cc98646292206cd3e86f74d88b4dcc1d937088645e9b0cbca84b7ce74eb", "0x1");
      return true;
    }
      ADD_CHECKPOINT2(0,       "1323aa63f4789ae2c87a55020661c7224432f56a7ae74e8b20958bae95fcf7ee", "0x1");
      ADD_CHECKPOINT2(500,     "ec0232324f503db6962a453eaa2eedd2084487a4c6db9aec90b3464229c20665", "0x1181d0");
      ADD_CHECKPOINT2(1000,    "8234f234ba3a155176048e1cb0ec72c80148080e74fb5d45de9c7b730847a08d", "0x14bef2");
      ADD_CHECKPOINT2(1500,    "e9435da0d22297ce35105cce7e7de62510ed8120567626ed619bcb02fabcdacd", "0x16ca9a");
      ADD_CHECKPOINT2(2000,    "9090e5a4a06ddc3cbf6a624f75884ef532f00dddde18478c7ede057ec2dc7c0b", "0x1905dc");
      ADD_CHECKPOINT2(2500,    "183e841f34d2fc60d29ee2e52dab6eb48e6234b0f1c349d61601ac429d8a40b7", "0x2a09890");
      ADD_CHECKPOINT2(3000,    "1b45f342e44a9007c39396a0b8b2f57e5ae251f8e7f9c05f73e77a7ca9e204b6", "0x8b76080");
      ADD_CHECKPOINT2(3500,    "7e5fc7016e721bef5e858ce111eacf006f4c16f0ae2bc2a95ec4c3021d6c139f", "0x1abdee75");
      ADD_CHECKPOINT2(4000,    "43fadaf4975de4ea5437216eb76571956319baf491af6ec4cda106266065b748", "0x41be8f2b");
      ADD_CHECKPOINT2(4500,    "1c53b5867f0f5032bed8b09313b5dc7bfa78aa14952fc5dd649975836abb1a9d", "0xcffbc43e");
      ADD_CHECKPOINT2(5000,    "8707e5bdd5fcf79e3a7a0e1f86d6bd13d41633e8a5cb1d9152fd2ee1c09363d1", "0x23a784c00");
      ADD_CHECKPOINT2(5500,    "9d0f8d423a0b465b59715405e46dbd1268afbe5e90820fa206edce02ce2c9533", "0x413058c16");
      ADD_CHECKPOINT2(6000,    "e31a189978d53ffe8a09aaab20b1b4a8a2e3498efbcbd98268ef1e80a1344cc4", "0x6282f01bc");
      ADD_CHECKPOINT2(6500,    "7cb5eddc9be4f5125535bb7d70aebd3a6641487b9c420802507a079322a34749", "0x96510a043");
      ADD_CHECKPOINT2(7000,    "ba212ed7977793d01388685399d698d1b93c593429cb6fefc6c0138193ba8abf", "0xd3e166db4");
      ADD_CHECKPOINT2(7500,    "5e308c3fd496ce497e147b0c3f099cf6584f77eeda73548b71d11b01eb5df87d", "0x113c2c6365");
      ADD_CHECKPOINT2(8000,    "bce52d1f1cf57891d0d1cbe847d2cd5c85c24a32a85fcf9c5ab89869f670b33b", "0x14a84ec875");
      ADD_CHECKPOINT2(8500,    "7fd7b2a5c86d0df0cccd9e8e101d5dcd78c87b3278d4094ff9bf374b63675689", "0x18485103b7");
      ADD_CHECKPOINT2(9000,    "1fdc5df60d77b2d9ee4854381d3e4bdfea5f543c6b79240c87f5b3ee8e502994", "0x1befe8104c");
      ADD_CHECKPOINT2(9500,    "5a834ae80fcc3982011ebe6ab68af56cb630781c6e5c785ef9f94c99a6cb7392", "0x1f90fa29b9");
      ADD_CHECKPOINT2(10000,   "064b0631e66474f96d021cef287518b8302a6bdcb5d1027186c2844fd0ce2e84", "0x2338a1d5c1");
      ADD_CHECKPOINT2(10500,   "95d6b7e13929271aa3972f81327d537a876f7ede306ca9be2c796713ea7ffb6f", "0x270ad09f09");
      ADD_CHECKPOINT2(11000,   "4d3fe1456375a6eae6a549354dcc670af038718ec7332d61f87d305e0285f6ed", "0x2abae6e67b");
      ADD_CHECKPOINT2(11500,   "fb2a625fd5b87273a9ec08efbc7d06d726c515a4beb5d04bf15115ea2907f5c1", "0x2d02a5c842");
      ADD_CHECKPOINT2(12000,   "bfd0eb6b3c7d6e9b843ab257d6909c6bce75472068449b9ccb8a07e40e52aeab", "0x2e982f0774");
      ADD_CHECKPOINT2(12500,   "8c75b60937976ec514137eda7bb3b63cbabbcb7e02c365c923840ec2f9f2090e", "0x307e8de8a7");
      ADD_CHECKPOINT2(13000,   "9ad45619973fe23a7968f34372c61b6a1df2b670e0f5980902a4a8f682ffe730", "0x34530a120f");
      ADD_CHECKPOINT2(13500,   "ec437b4ab27a0fcae2912c21596f9b1fbcb7eee3df2d74385c60f2b9b5e8693f", "0x3b795841ef");
      ADD_CHECKPOINT2(14000,   "15e841d9b12acb0103d4a5f962706b89ffdbd4e4669f9f5be09edb55f00919d3", "0x450a52572d");
      ADD_CHECKPOINT2(14500,   "93959a52284923c26578ed3ec7f7623dad9f3459d958ba7dfad01690cb7fb6e8", "0x53f5e65092");
      ADD_CHECKPOINT2(15000,   "f0957fcfb3e7f452893272b043ee99cc68703c9aa97efdfb23acaa883d4c0b4f", "0x65ec423ad8");
      ADD_CHECKPOINT2(15500,   "804a67b2cb7f86707347b28e3a487e14c14970c3e175d7e46f6933b3c9627725", "0x7c0be53dae");
      ADD_CHECKPOINT2(16000,   "d4afd954e4445312ea64473ce2ce06444a3370624143e1d6129ca75216f23b8e", "0x97a9e3e315");
      ADD_CHECKPOINT2(16500,   "650ccb665eec209a4b342d395c90637cca154f27b3699cafb39032fa3bbcf58a", "0xb7a6a197e1");
    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for (std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
	LOG_PRINT_L1("ignoring checkpoint height " << height);
      } else {
	std::string blockhash = it->hash;
	LOG_PRINT_L1("Adding checkpoint height " << height << ", hash=" << blockhash);
	ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

  bool checkpoints::load_checkpoints_from_dns(network_type nettype)
  {
    std::vector<std::string> records;
     return false;
    // All four LozzaxPulse domains have DNSSEC on and valid
    static const std::vector<std::string> dns_urls = {
    };

    static const std::vector<std::string> testnet_dns_urls = {
    };

    static const std::vector<std::string> stagenet_dns_urls = {
    };

    if (!tools::dns_utils::load_txt_records_from_dns(records, nettype == TESTNET ? testnet_dns_urls : nettype == STAGENET ? stagenet_dns_urls : dns_urls))
      return true; // why true ?

    for (const auto& record : records)
    {
      auto pos = record.find(":");
      if (pos != std::string::npos)
      {
        uint64_t height;
        crypto::hash hash;

        // parse the first part as uint64_t,
        // if this fails move on to the next record
        std::stringstream ss(record.substr(0, pos));
        if (!(ss >> height))
        {
    continue;
        }

        // parse the second part as crypto::hash,
        // if this fails move on to the next record
        std::string hashStr = record.substr(pos + 1);
        if (!epee::string_tools::hex_to_pod(hashStr, hash))
        {
    continue;
        }

        ADD_CHECKPOINT(height, hashStr);
      }
    }
    return false; //disable dnscheckpoints
  }

  bool checkpoints::load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype, bool dns)
  {
    bool result;

    result = load_checkpoints_from_json(json_hashfile_fullpath);
    if (dns)
    {
      result &= load_checkpoints_from_dns(nettype);
    }

    return result;
  }
}
