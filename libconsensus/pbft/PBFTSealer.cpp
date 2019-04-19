/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */

/**
 * @brief : implementation of PBFT consensus
 * @file: PBFTSealer.cpp
 * @author: yujiechen
 * @date: 2018-09-28
 *
 * @author: yujiechen
 * @file:PBFTSealer.cpp
 * @date: 2018-10-26
 * @modifications: rename PBFTSealer.cpp to PBFTSealer.cpp
 */
#include "PBFTSealer.h"
#include <libdevcore/CommonJS.h>
#include <libdevcore/Worker.h>
#include <libethcore/CommonJS.h>
using namespace dev::eth;
using namespace dev::db;
using namespace dev::blockverifier;
using namespace dev::blockchain;
using namespace dev::p2p;
namespace dev
{
namespace consensus
{
void PBFTSealer::handleBlock()
{
    /// check the max transaction num of a block early when generate new block
    /// in case of the block is generated by the nextleader and the transaction num is over
    /// maxTransactionLimit
    if (m_sealing.block.getTransactionSize() > m_pbftEngine->maxBlockTransactions())
    {
        PBFTSEALER_LOG(DEBUG)
            << LOG_DESC("Drop block for the transaction num is over maxTransactionLimit")
            << LOG_KV("transaction_num", m_sealing.block.getTransactionSize())
            << LOG_KV("maxTransactionLimit", m_pbftEngine->maxBlockTransactions());
        resetSealingBlock();
        /// notify to re-generate the block
        m_signalled.notify_all();
        m_blockSignalled.notify_all();
        return;
    }
    setBlock();
    PBFTSEALER_LOG(INFO) << LOG_DESC("++++++++++++++++ Generating seal on")
                         << LOG_KV("blkNum", m_sealing.block.header().number())
                         << LOG_KV("tx", m_sealing.block.getTransactionSize())
                         << LOG_KV("nodeIdx", m_pbftEngine->nodeIdx())
                         << LOG_KV("hash", m_sealing.block.header().hash().abridged());
    m_pbftEngine->generatePrepare(m_sealing.block);
    if (m_pbftEngine->shouldReset(m_sealing.block))
    {
        resetSealingBlock();
        m_signalled.notify_all();
        m_blockSignalled.notify_all();
    }
}
void PBFTSealer::setBlock()
{
    m_sealing.block.header().populateFromParent(
        m_blockChain->getBlockByNumber(m_blockChain->number())->header());
    resetSealingHeader(m_sealing.block.header());
    m_sealing.block.calTransactionRoot();
}

/**
 * @brief: this node can generate block or not
 * @return true: this node can generate block
 * @return false: this node can't generate block
 */
bool PBFTSealer::shouldSeal()
{
    return Sealer::shouldSeal() && m_pbftEngine->shouldSeal();
}
void PBFTSealer::start()
{
    m_pbftEngine->start();
    Sealer::start();
}
void PBFTSealer::stop()
{
    Sealer::stop();
    m_pbftEngine->stop();
}

/// decrease maxBlockCanSeal to half when timeout
void PBFTSealer::calculateMaxPackTxNum(uint64_t& maxBlockCanSeal)
{
    if (m_pbftEngine->view() > 0 && m_pbftEngine->timeout() &&
        m_lastView != (m_pbftEngine->view()) && maxBlockCanSeal >= 2)
    {
        PBFTSEALER_LOG(DEBUG) << LOG_DESC("decrease maxBlockCanSeal to half for PBFT timeout")
                              << LOG_KV("org_maxBlockCanSeal", maxBlockCanSeal)
                              << LOG_KV("halfed_maxBlockCanSeal", maxBlockCanSeal / 2);
        maxBlockCanSeal /= 2;
        m_lastView = m_pbftEngine->view();
    }
    else if (!m_pbftEngine->timeout() && m_pbftEngine->view() == 0)
    {
        m_lastView = 0;
        if (maxBlockCanSeal < m_pbftEngine->maxBlockTransactions())
        {
            maxBlockCanSeal += (0.5 * maxBlockCanSeal);
            PBFTSEALER_LOG(DEBUG) << LOG_DESC("increase maxBlockCanSeal")
                                  << LOG_KV("org_maxBlockCanSeal", maxBlockCanSeal)
                                  << LOG_KV("increased_maxBlockCanSeal", maxBlockCanSeal);
            if (maxBlockCanSeal > m_pbftEngine->maxBlockTransactions())
            {
                maxBlockCanSeal = m_pbftEngine->maxBlockTransactions();
            }
        }
    }
}

}  // namespace consensus
}  // namespace dev
