/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _RTW_HE_C

#include <drv_types.h>

#ifdef CONFIG_80211AX_HE

/* for now cover BW 20/40/80 bounded in 2SS */
const u16 HE_MCS_DATA_RATE[3][MAX_HE_GI_TYPE][MAX_HE_MCS_INDEX] = {
	{	/* 20M */
		{	/* 3.2us */
			14, 29, 43, 58, 87, 117, 131, 146, 175, 195, 219, 243,
			29, 58, 87, 117, 175, 234, 263, 292, 351, 390, 438, 487,
		},
		{	/* 1.6us */
			16, 32, 48, 65, 97, 130, 146, 162, 195, 216, 243, 270,
			32, 65, 97, 130, 195, 260, 292, 325, 390, 433, 487, 541,
		},
		{	/* 0.8us */
			17, 34, 51, 68, 103, 137, 154, 172, 206, 229, 258, 286,
			34, 68, 103, 137, 206, 275, 309, 344, 413, 458, 516, 573,
		}
	},
	{	/* 40M */
		{	/* 3.2us */
			29, 58, 87, 117, 175, 234, 263, 292, 351, 390, 438, 487,
			58, 117, 175, 234, 351, 468, 526, 585, 702, 780, 877, 975,
		},
		{	/* 1.6us */
			32, 65, 97, 130, 195, 260, 292, 325, 390, 433, 487, 541,
			65, 130, 195, 260, 390, 520, 585, 650, 780, 866, 975, 1083,
		},
		{	/* 0.8us */
			34, 68, 103, 138, 206, 275, 309, 344, 413, 458, 516, 573,
			68, 137, 206, 275, 413, 550, 619, 688, 825, 917, 1032, 1147,
		}
	},
	{	/* 80M */
		{	/* 3.2us */
			61, 122, 183, 245, 367, 490, 551, 612, 735, 816, 918, 1020,
			122, 245, 367, 490, 735, 980, 1102, 1225, 1470, 1633, 1839, 2041,
		},
		{	/* 1.6us */
			68, 136, 204, 272, 408, 544, 612, 680, 816, 907, 1020, 1134,
			136, 272, 408, 544, 816, 1088, 1225, 1361, 1633, 1814, 2041, 2268,
		},
		{	/* 0.8us */
			72, 144, 216, 288, 432, 576, 648, 720, 864, 960, 1080, 1200,
			144, 288, 432, 576, 864, 1153, 1297, 1441, 1729, 1921, 2161, 2402,
		}
	}
};

u16 rtw_he_mcs_to_data_rate(u8 bw, u8 guard_int, u8 he_mcs_rate)
{
	u8 gi = 2; /* use 0.8us GI since 2XLTF_0.8us GI is mandatory in HE */
	u8 mcs_idx = he_mcs_rate - MGN_HE1SS_MCS0;

	return HE_MCS_DATA_RATE[bw][gi][mcs_idx];
}

static u8 rtw_he_get_highest_rate(u8 *he_mcs_map)
{
	u8 i, j;
	u8 bit_map;
	u8 he_mcs_rate = 0;

	/* currently only consider the BW 80M */
	for (i = 0; i < 2; i++) {
		if (he_mcs_map[i] != 0xff) {
			/* max to 4SS, each SS contains 2 bit */
			for (j = 0; j < 8; j += 2) {
				bit_map = (he_mcs_map[i] >> j) & 3;

				if (bit_map != 3)
					he_mcs_rate = MGN_HE1SS_MCS7 + 12 * j / 2 + i * 48 + 2 * bit_map;
			}
		}
	}

	/*RTW_INFO("####### HighestHEMCSRate is %x\n", he_mcs_rate);*/
	return he_mcs_rate;
}

void	rtw_he_use_default_setting(_adapter *padapter)
{
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *role_cap = &(wrole->proto_role_cap);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct he_priv *phepriv = &pmlmepriv->hepriv;

	phepriv->he_highest_rate = rtw_he_get_highest_rate(role_cap->he_rx_mcs);
}

static void rtw_he_set_asoc_cap_supp_mcs(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start, u8 supp_mcs_len)
{
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *role_cap = &(wrole->proto_role_cap);
	int nss = 0, nss_tx = 0, nss_rx = 0;
	u8 mcs_from_role = HE_MSC_NOT_SUPP;
	u8 mcs_from_ie = HE_MSC_NOT_SUPP;
	u8 mcs_val_rx = HE_MSC_NOT_SUPP;
	u8 mcs_val_tx = HE_MSC_NOT_SUPP;

	_rtw_memset(phl_sta->asoc_cap.he_rx_mcs, HE_MSC_NOT_SUPP_BYTE, HE_CAP_ELE_SUPP_MCS_LEN_RX_80M);
	_rtw_memset(phl_sta->asoc_cap.he_tx_mcs, HE_MSC_NOT_SUPP_BYTE, HE_CAP_ELE_SUPP_MCS_LEN_TX_80M);

	/* only deal with <= 80MHz now */
	for (nss = 1; nss <= 8; nss++) {

		mcs_val_rx = HE_MSC_NOT_SUPP;
		mcs_val_tx = HE_MSC_NOT_SUPP;

		switch (nss) {
		case 1:
			mcs_from_role = GET_HE_CAP_MCS_1SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_1SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_1SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_1SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_1SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_1SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 2:
			mcs_from_role = GET_HE_CAP_MCS_2SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_2SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_2SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_2SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_2SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_2SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 3:
			mcs_from_role = GET_HE_CAP_MCS_3SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_3SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_3SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_3SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_3SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_3SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 4:
			mcs_from_role = GET_HE_CAP_MCS_4SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_4SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_4SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_4SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_4SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_4SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 5:
			mcs_from_role = GET_HE_CAP_MCS_5SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_5SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_5SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_5SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_5SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_5SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 6:
			mcs_from_role = GET_HE_CAP_MCS_6SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_6SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_6SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_6SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_6SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_6SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 7:
			mcs_from_role = GET_HE_CAP_MCS_7SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_7SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_7SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_7SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_7SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_7SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		case 8:
			mcs_from_role = GET_HE_CAP_MCS_8SS(role_cap->he_tx_mcs);
			mcs_from_ie = GET_HE_CAP_RX_MCS_LESS_THAN_80MHZ_8SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_rx = (mcs_from_role < mcs_from_ie) ? mcs_from_role : mcs_from_ie;

			mcs_from_role = GET_HE_CAP_MCS_8SS(role_cap->he_rx_mcs);
			mcs_from_ie = GET_HE_CAP_TX_MCS_LESS_THAN_80MHZ_8SS(ele_start);
			if ((mcs_from_role != HE_MSC_NOT_SUPP) && (mcs_from_ie != HE_MSC_NOT_SUPP))
				mcs_val_tx = mcs_from_ie;

			SET_HE_CAP_MCS_8SS(phl_sta->asoc_cap.he_rx_mcs, mcs_val_rx);
			SET_HE_CAP_MCS_8SS(phl_sta->asoc_cap.he_tx_mcs, mcs_val_tx);
			break;
		}

		if (mcs_val_rx != HE_MSC_NOT_SUPP)
			nss_rx++;

		if (mcs_val_tx != HE_MSC_NOT_SUPP)
			nss_tx++;
	}

	phl_sta->asoc_cap.nss_rx = nss_rx;
	phl_sta->asoc_cap.nss_tx = nss_tx;
}

static void rtw_he_set_asoc_cap_ppe_thre(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start)
{
	u8 nsts, rumsk, i, j, offset, shift;
	u16 ppe8, ppe16;

	if (phl_sta->asoc_cap.pkt_padding != 3)
		return;

	nsts = GET_HE_CAP_PPE_NSTS(ele_start);
	rumsk = GET_HE_CAP_PPE_PU_IDX_BITMASK(ele_start);
	shift = 7;

	for (i = 0; i <= nsts; i ++) {
		for (j = 0; j < 4; j++) {
			if (rumsk & (BIT(0) << j)) {
				offset = shift / 8;
				ppe16 = LE_BITS_TO_2BYTE(ele_start + offset, shift % 8, 3);
				shift += 3;
				offset = shift / 8;
				ppe8 = LE_BITS_TO_2BYTE(ele_start + offset, shift % 8, 3);
				shift += 3;
				phl_sta->asoc_cap.ppe_thr[i][j] = ((ppe16 & 0x07) | ((ppe8 & 0x07) << 3));
			} else {
				phl_sta->asoc_cap.ppe_thr[i][j] = 0;
			}
		}
	}
}

static void update_sta_he_mac_cap_apmode(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start)
{
	/* CONFIG_80211AX_HE_TODO - we may need to refer to role_cap when setting some of asoc_cap  */
#if 0
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *role_cap = &(wrole->proto_role_cap);
#endif

	phl_sta->asoc_cap.htc_rx = GET_HE_MAC_CAP_HTC_HE_SUPPORT(ele_start);
	phl_sta->asoc_cap.twt = GET_HE_MAC_CAP_TWT_REQUESTER_SUPPORT(ele_start);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_TWT_RESPONDER_SUPPORT(ele_start)) << 1);
	phl_sta->asoc_cap.trig_padding = GET_HE_MAC_CAP_TRI_FRAME_PADDING_DUR(ele_start);
	phl_sta->asoc_cap.all_ack = GET_HE_MAC_CAP_ALL_ACK_SUPPORT(ele_start);
	phl_sta->asoc_cap.a_ctrl = GET_HE_MAC_CAP_TRS_SUPPORT(ele_start);
	phl_sta->asoc_cap.a_ctrl |= ((GET_HE_MAC_CAP_BRS_SUPPORT(ele_start)) << 1);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_BC_TWT_SUPPORT(ele_start)) << 2);
	phl_sta->asoc_cap.a_ctrl |= ((GET_HE_MAC_CAP_OM_CTRL_SUPPORT(ele_start)) << 2);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_FLEX_TWT_SCHED_SUPPORT(ele_start)) << 3);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_PSR_RESPONDER(ele_start)) << 4);
	phl_sta->asoc_cap.ops = GET_HE_MAC_CAP_OPS_SUPPORT(ele_start);
	phl_sta->asoc_cap.amsdu_in_ampdu =
		GET_HE_MAC_CAP_AMSDU_NOT_UNDER_BA_IN_ACK_EN_AMPDU(ele_start);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_HE_SUB_CH_SELECTIVE_TX(ele_start)) << 5);
	phl_sta->asoc_cap.ht_vht_trig_rx =
		GET_HE_MAC_CAP_HT_VHT_TRIG_FRAME_RX(ele_start);
}

static void update_sta_he_phy_cap_apmode(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start, u8 *supp_mcs_len)
{
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *role_cap = &(wrole->proto_role_cap);
	struct role_cap_t *cap = &(wrole->cap);

	if (phl_sta->chandef.band == BAND_ON_24G) {
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(0))
			phl_sta->chandef.bw = (wrole->chandef.bw < CHANNEL_WIDTH_40) ? 
			wrole->chandef.bw : CHANNEL_WIDTH_40;
	} else if (phl_sta->chandef.band == BAND_ON_5G) {
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(1))
			phl_sta->chandef.bw = (wrole->chandef.bw < CHANNEL_WIDTH_80) ?
			wrole->chandef.bw : CHANNEL_WIDTH_80;
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(2))
			*supp_mcs_len += 4;
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(3))
			*supp_mcs_len += 4;
	}

	phl_sta->asoc_cap.he_ldpc = (GET_HE_PHY_CAP_LDPC_IN_PAYLOAD(ele_start) & role_cap->he_ldpc);

	if (phl_sta->asoc_cap.er_su) {
		phl_sta->asoc_cap.ltf_gi = (BIT(RTW_GILTF_2XHE16) |
			BIT(RTW_GILTF_2XHE08) | BIT(RTW_GILTF_1XHE16) |
			(GET_HE_PHY_CAP_NDP_4X_LTF_3_POINT_2_GI(ele_start) ?
			BIT(RTW_GILTF_LGI_4XHE32) : 0) |
			(GET_HE_PHY_CAP_ERSU_PPDU_4X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_SGI_4XHE08) : 0) |
			(GET_HE_PHY_CAP_ERSU_PPDU_1X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_1XHE08) : 0));
	} else {
		phl_sta->asoc_cap.ltf_gi = (BIT(RTW_GILTF_2XHE16) |
			BIT(RTW_GILTF_2XHE08) | BIT(RTW_GILTF_1XHE16) |
			(GET_HE_PHY_CAP_NDP_4X_LTF_3_POINT_2_GI(ele_start) ?
			BIT(RTW_GILTF_LGI_4XHE32) : 0) |
			(GET_HE_PHY_CAP_SU_MU_PPDU_4X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_SGI_4XHE08) : 0) |
			(GET_HE_PHY_CAP_SU_PPDU_1X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_1XHE08) : 0));
	}

	phl_sta->asoc_cap.stbc_tx = (GET_HE_PHY_CAP_STBC_TX_LESS_THAN_80MHZ(ele_start) & role_cap->stbc_he_rx);
	phl_sta->asoc_cap.stbc_he_rx = (GET_HE_PHY_CAP_STBC_RX_LESS_THAN_80MHZ(ele_start) & cap->tx_he_stbc);
	phl_sta->asoc_cap.doppler_tx = (GET_HE_PHY_CAP_DOPPLER_TX(ele_start) & role_cap->doppler_rx);
	phl_sta->asoc_cap.doppler_rx = (GET_HE_PHY_CAP_DOPPLER_RX(ele_start) & role_cap->doppler_tx);

	phl_sta->asoc_cap.dcm_max_const_tx =
		GET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_TX(ele_start);
	if (phl_sta->asoc_cap.dcm_max_const_tx > role_cap->dcm_max_const_rx)
		phl_sta->asoc_cap.dcm_max_const_tx = role_cap->dcm_max_const_rx;

	phl_sta->asoc_cap.dcm_max_nss_tx = (GET_HE_PHY_CAP_DCM_MAX_NSS_TX(ele_start) & role_cap->dcm_max_nss_rx);

	phl_sta->asoc_cap.dcm_max_const_rx =
		GET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_RX(ele_start);
	if (phl_sta->asoc_cap.dcm_max_const_rx > role_cap->dcm_max_const_tx)
		phl_sta->asoc_cap.dcm_max_const_rx = role_cap->dcm_max_const_tx;

	phl_sta->asoc_cap.dcm_max_nss_rx = (GET_HE_PHY_CAP_DCM_MAX_NSS_RX(ele_start) & role_cap->dcm_max_nss_tx);

	phl_sta->asoc_cap.partial_bw_su_er =
		GET_HE_PHY_CAP_RX_PARTIAL_BW_SU_IN_20MHZ_MUPPDU(ele_start);
	phl_sta->asoc_cap.he_su_bfmr = GET_HE_PHY_CAP_SU_BFER(ele_start);
	phl_sta->asoc_cap.he_su_bfme = GET_HE_PHY_CAP_SU_BFEE(ele_start);
	phl_sta->asoc_cap.he_mu_bfmr = GET_HE_PHY_CAP_MU_BFER(ele_start);
	phl_sta->asoc_cap.bfme_sts =
		GET_HE_PHY_CAP_BFEE_STS_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.bfme_sts_greater_80mhz =
		GET_HE_PHY_CAP_BFEE_STS_GREATER_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.num_snd_dim =
		GET_HE_PHY_CAP_NUM_SND_DIMEN_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.num_snd_dim_greater_80mhz =
		GET_HE_PHY_CAP_NUM_SND_DIMEN_GREATER_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.ng_16_su_fb = GET_HE_PHY_CAP_NG_16_SU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.ng_16_mu_fb = GET_HE_PHY_CAP_NG_16_MU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.cb_sz_su_fb =
		GET_HE_PHY_CAP_CODEBOOK_4_2_SU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.cb_sz_mu_fb =
		GET_HE_PHY_CAP_CODEBOOK_7_5_MU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_su_bfm_fb =
		GET_HE_PHY_CAP_TRIG_SUBF_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_mu_bfm_fb =
		GET_HE_PHY_CAP_TRIG_MUBF_PARTIAL_BW_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_cqi_fb = GET_HE_PHY_CAP_TRIG_CQI_FEEDBACK(ele_start);
	phl_sta->asoc_cap.partial_bw_su_er =
		GET_HE_PHY_CAP_PARTIAL_BW_EXT_RANGE(ele_start);
	phl_sta->asoc_cap.pwr_bst_factor =
		GET_HE_PHY_CAP_PWR_BOOST_FACTOR_SUPPORT(ele_start);
	phl_sta->asoc_cap.max_nc = GET_HE_PHY_CAP_MAX_NC(ele_start);
	phl_sta->asoc_cap.stbc_tx_greater_80mhz =
		(GET_HE_PHY_CAP_STBC_TX_GREATER_THAN_80MHZ(ele_start) & role_cap->stbc_rx_greater_80mhz);
	phl_sta->asoc_cap.stbc_rx_greater_80mhz =
		(GET_HE_PHY_CAP_STBC_RX_GREATER_THAN_80MHZ(ele_start) & role_cap->stbc_tx_greater_80mhz);
	phl_sta->asoc_cap.dcm_max_ru = GET_HE_PHY_CAP_DCM_MAX_RU(ele_start);
	phl_sta->asoc_cap.long_sigb_symbol =
		GET_HE_PHY_CAP_LONGER_THAN_16_HESIGB_OFDM_SYM(ele_start);
	phl_sta->asoc_cap.non_trig_cqi_fb =
		GET_HE_PHY_CAP_NON_TRIGGER_CQI_FEEDBACK(ele_start);
	phl_sta->asoc_cap.tx_1024q_ru =
		(GET_HE_PHY_CAP_TX_1024_QAM_LESS_THAN_242_TONE_RU(ele_start) & role_cap->rx_1024q_ru);
	phl_sta->asoc_cap.rx_1024q_ru =
		(GET_HE_PHY_CAP_RX_1024_QAM_LESS_THAN_242_TONE_RU(ele_start) & role_cap->tx_1024q_ru);
	phl_sta->asoc_cap.fbw_su_using_mu_cmprs_sigb =
		GET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_CMP_SIGB(ele_start);
	phl_sta->asoc_cap.fbw_su_using_mu_non_cmprs_sigb =
		GET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_NONCMP_SIGB(ele_start);

	if (GET_HE_PHY_CAP_PPE_THRESHOLD_PRESENT(ele_start))
		phl_sta->asoc_cap.pkt_padding = 3;
	else
		phl_sta->asoc_cap.pkt_padding = GET_HE_PHY_CAP_NOMINAL_PACKET_PADDING(ele_start);
}

static void update_sta_he_supp_mcs_apmode(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start, u8 supp_mcs_len)
{
	rtw_he_set_asoc_cap_supp_mcs(padapter, phl_sta, ele_start, supp_mcs_len);
}

static void update_sta_he_ppe_thre_apmode(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start)
{
	rtw_he_set_asoc_cap_ppe_thre(padapter, phl_sta, ele_start);
}

void	update_sta_he_info_apmode(_adapter *padapter, void *sta)
{
	struct sta_info	*psta = (struct sta_info *)sta;
	struct rtw_phl_stainfo_t *phl_sta = psta->phl_sta;
	struct he_priv	*phepriv_sta = &psta->hepriv;
	u8 *ele_start = NULL;
	u8 supp_mcs_len = 4;

	if (phepriv_sta->he_option == _FALSE)
		return;

	ele_start = &(phepriv_sta->he_cap[1]);
	update_sta_he_mac_cap_apmode(padapter, phl_sta, ele_start);

	ele_start += HE_CAP_ELE_MAC_CAP_LEN;
	update_sta_he_phy_cap_apmode(padapter, phl_sta, ele_start, &supp_mcs_len);

	ele_start += HE_CAP_ELE_PHY_CAP_LEN;
	update_sta_he_supp_mcs_apmode(padapter, phl_sta, ele_start, supp_mcs_len);

	ele_start += supp_mcs_len;
	update_sta_he_ppe_thre_apmode(padapter, phl_sta, ele_start);
}

void	update_hw_he_param(_adapter *padapter)
{
	/* CONFIG_80211AX_HE_TODO */
}

static void HE_mac_caps_handler(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start)
{
	phl_sta->asoc_cap.htc_rx = GET_HE_MAC_CAP_HTC_HE_SUPPORT(ele_start);
	phl_sta->asoc_cap.twt = GET_HE_MAC_CAP_TWT_REQUESTER_SUPPORT(ele_start);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_TWT_RESPONDER_SUPPORT(ele_start)) << 1);
	phl_sta->asoc_cap.trig_padding = GET_HE_MAC_CAP_TRI_FRAME_PADDING_DUR(ele_start);
	phl_sta->asoc_cap.all_ack = GET_HE_MAC_CAP_ALL_ACK_SUPPORT(ele_start);
	phl_sta->asoc_cap.a_ctrl = GET_HE_MAC_CAP_TRS_SUPPORT(ele_start);
	phl_sta->asoc_cap.a_ctrl |= ((GET_HE_MAC_CAP_BRS_SUPPORT(ele_start)) << 1);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_BC_TWT_SUPPORT(ele_start)) << 2);
	phl_sta->asoc_cap.a_ctrl |= ((GET_HE_MAC_CAP_OM_CTRL_SUPPORT(ele_start)) << 2);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_FLEX_TWT_SCHED_SUPPORT(ele_start)) << 3);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_PSR_RESPONDER(ele_start)) << 4);
	phl_sta->asoc_cap.ops = GET_HE_MAC_CAP_OPS_SUPPORT(ele_start);
	phl_sta->asoc_cap.amsdu_in_ampdu =
		GET_HE_MAC_CAP_AMSDU_NOT_UNDER_BA_IN_ACK_EN_AMPDU(ele_start);
	phl_sta->asoc_cap.twt |= ((GET_HE_MAC_CAP_HE_SUB_CH_SELECTIVE_TX(ele_start)) << 5);
	phl_sta->asoc_cap.ht_vht_trig_rx =
		GET_HE_MAC_CAP_HT_VHT_TRIG_FRAME_RX(ele_start);
}

static void HE_phy_caps_handler(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start, u8 *supp_mcs_len)
{
	struct rtw_wifi_role_t 	*wrole = padapter->phl_role;

	if (phl_sta->chandef.band == BAND_ON_24G) {
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(0))
			phl_sta->chandef.bw = (wrole->chandef.bw < CHANNEL_WIDTH_40) ?
			wrole->chandef.bw : CHANNEL_WIDTH_40;
	} else if (phl_sta->chandef.band == BAND_ON_5G) {
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(1))
			phl_sta->chandef.bw = (wrole->chandef.bw < CHANNEL_WIDTH_80) ?
			wrole->chandef.bw : CHANNEL_WIDTH_80;
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(2))
			*supp_mcs_len += 4;
		if (GET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(ele_start) & BIT(3))
			*supp_mcs_len += 4;
	}
	phl_sta->asoc_cap.he_ldpc = GET_HE_PHY_CAP_LDPC_IN_PAYLOAD(ele_start);
	if (phl_sta->asoc_cap.er_su) {
		phl_sta->asoc_cap.ltf_gi = (BIT(RTW_GILTF_2XHE16) |
			BIT(RTW_GILTF_2XHE08) | BIT(RTW_GILTF_1XHE16) |
			(GET_HE_PHY_CAP_NDP_4X_LTF_3_POINT_2_GI(ele_start) ?
			BIT(RTW_GILTF_LGI_4XHE32) : 0) |
			(GET_HE_PHY_CAP_ERSU_PPDU_4X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_SGI_4XHE08) : 0) |
			(GET_HE_PHY_CAP_ERSU_PPDU_1X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_1XHE08) : 0));
	} else {
		phl_sta->asoc_cap.ltf_gi = (BIT(RTW_GILTF_2XHE16) |
			BIT(RTW_GILTF_2XHE08) | BIT(RTW_GILTF_1XHE16) |
			(GET_HE_PHY_CAP_NDP_4X_LTF_3_POINT_2_GI(ele_start) ?
			BIT(RTW_GILTF_LGI_4XHE32) : 0) |
			(GET_HE_PHY_CAP_SU_MU_PPDU_4X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_SGI_4XHE08) : 0) |
			(GET_HE_PHY_CAP_SU_PPDU_1X_LTF_0_POINT_8_GI(ele_start) ?
			BIT(RTW_GILTF_1XHE08) : 0));
	}
	phl_sta->asoc_cap.stbc_tx = GET_HE_PHY_CAP_STBC_TX_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.stbc_he_rx = GET_HE_PHY_CAP_STBC_RX_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.doppler_tx = GET_HE_PHY_CAP_DOPPLER_TX(ele_start);
	phl_sta->asoc_cap.doppler_rx = GET_HE_PHY_CAP_DOPPLER_RX(ele_start);
	phl_sta->asoc_cap.dcm_max_const_tx =
		GET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_TX(ele_start);
	phl_sta->asoc_cap.dcm_max_nss_tx = GET_HE_PHY_CAP_DCM_MAX_NSS_TX(ele_start);
	phl_sta->asoc_cap.dcm_max_const_rx =
		GET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_RX(ele_start);
	phl_sta->asoc_cap.dcm_max_nss_rx = GET_HE_PHY_CAP_DCM_MAX_NSS_RX(ele_start);
	phl_sta->asoc_cap.partial_bw_su_er =
		GET_HE_PHY_CAP_RX_PARTIAL_BW_SU_IN_20MHZ_MUPPDU(ele_start);
	phl_sta->asoc_cap.he_su_bfmr = GET_HE_PHY_CAP_SU_BFER(ele_start);
	phl_sta->asoc_cap.he_su_bfme = GET_HE_PHY_CAP_SU_BFEE(ele_start);
	phl_sta->asoc_cap.he_mu_bfmr = GET_HE_PHY_CAP_MU_BFER(ele_start);
	phl_sta->asoc_cap.bfme_sts =
		GET_HE_PHY_CAP_BFEE_STS_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.bfme_sts_greater_80mhz =
		GET_HE_PHY_CAP_BFEE_STS_GREATER_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.num_snd_dim =
		GET_HE_PHY_CAP_NUM_SND_DIMEN_LESS_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.num_snd_dim_greater_80mhz =
		GET_HE_PHY_CAP_NUM_SND_DIMEN_GREATER_THAN_80MHZ(ele_start);

	RTW_INFO("%s: HE STA assoc_cap:\n", __func__);
	RTW_INFO("- SU BFer: %d\n", phl_sta->asoc_cap.he_su_bfmr);
	RTW_INFO("- SU BFee: %d\n", phl_sta->asoc_cap.he_su_bfme);
	RTW_INFO("- MU BFer: %d\n", phl_sta->asoc_cap.he_mu_bfmr);
	RTW_INFO("- BFee STS: %d\n", phl_sta->asoc_cap.bfme_sts);
	RTW_INFO("- BFee STS(>80MHz): %d\n", phl_sta->asoc_cap.bfme_sts_greater_80mhz);
	RTW_INFO("- BFer SND DIM number: %d\n", phl_sta->asoc_cap.num_snd_dim);
	RTW_INFO("- BFer SND DIM number(>80MHz): %d\n", phl_sta->asoc_cap.num_snd_dim_greater_80mhz);

	phl_sta->asoc_cap.ng_16_su_fb = GET_HE_PHY_CAP_NG_16_SU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.ng_16_mu_fb = GET_HE_PHY_CAP_NG_16_MU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.cb_sz_su_fb =
		GET_HE_PHY_CAP_CODEBOOK_4_2_SU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.cb_sz_mu_fb =
		GET_HE_PHY_CAP_CODEBOOK_7_5_MU_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_su_bfm_fb =
		GET_HE_PHY_CAP_TRIG_SUBF_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_mu_bfm_fb =
		GET_HE_PHY_CAP_TRIG_MUBF_PARTIAL_BW_FEEDBACK(ele_start);
	phl_sta->asoc_cap.trig_cqi_fb = GET_HE_PHY_CAP_TRIG_CQI_FEEDBACK(ele_start);
	phl_sta->asoc_cap.partial_bw_su_er =
		GET_HE_PHY_CAP_PARTIAL_BW_EXT_RANGE(ele_start);
	phl_sta->asoc_cap.pwr_bst_factor =
		GET_HE_PHY_CAP_PWR_BOOST_FACTOR_SUPPORT(ele_start);
	phl_sta->asoc_cap.max_nc = GET_HE_PHY_CAP_MAX_NC(ele_start);
	phl_sta->asoc_cap.stbc_tx_greater_80mhz =
		GET_HE_PHY_CAP_STBC_TX_GREATER_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.stbc_rx_greater_80mhz =
		GET_HE_PHY_CAP_STBC_RX_GREATER_THAN_80MHZ(ele_start);
	phl_sta->asoc_cap.dcm_max_ru = GET_HE_PHY_CAP_DCM_MAX_RU(ele_start);
	phl_sta->asoc_cap.long_sigb_symbol =
		GET_HE_PHY_CAP_LONGER_THAN_16_HESIGB_OFDM_SYM(ele_start);
	phl_sta->asoc_cap.non_trig_cqi_fb =
		GET_HE_PHY_CAP_NON_TRIGGER_CQI_FEEDBACK(ele_start);
	phl_sta->asoc_cap.tx_1024q_ru =
		GET_HE_PHY_CAP_TX_1024_QAM_LESS_THAN_242_TONE_RU(ele_start);
	phl_sta->asoc_cap.rx_1024q_ru =
		GET_HE_PHY_CAP_RX_1024_QAM_LESS_THAN_242_TONE_RU(ele_start);
	phl_sta->asoc_cap.fbw_su_using_mu_cmprs_sigb =
		GET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_CMP_SIGB(ele_start);
	phl_sta->asoc_cap.fbw_su_using_mu_non_cmprs_sigb =
		GET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_NONCMP_SIGB(ele_start);

	if (GET_HE_PHY_CAP_PPE_THRESHOLD_PRESENT(ele_start))
		phl_sta->asoc_cap.pkt_padding = 3;
	else
		phl_sta->asoc_cap.pkt_padding = GET_HE_PHY_CAP_NOMINAL_PACKET_PADDING(ele_start);
}

static void HE_supp_mcs_handler(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start, u8 supp_mcs_len)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct he_priv *phepriv = &pmlmepriv->hepriv;

	rtw_he_set_asoc_cap_supp_mcs(padapter, phl_sta, ele_start, supp_mcs_len);
	phepriv->he_highest_rate = rtw_he_get_highest_rate(phl_sta->asoc_cap.he_rx_mcs);
}

static void HE_ppe_thre_handler(_adapter *padapter, struct rtw_phl_stainfo_t *phl_sta, u8 *ele_start)
{
	rtw_he_set_asoc_cap_ppe_thre(padapter, phl_sta, ele_start);
}

void HE_caps_handler(_adapter *padapter, PNDIS_802_11_VARIABLE_IEs pIE)
{
	struct rtw_wifi_role_t 	*wrole = padapter->phl_role;
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct sta_priv 		*pstapriv = &padapter->stapriv;
	struct he_priv		*phepriv = &pmlmepriv->hepriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX 		*cur_network = &(pmlmeinfo->network);
	struct sta_info 		*psta = NULL;
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	u8 *ele_start = (&(pIE->data[0]) + 1);
	u8 supp_mcs_len = 4;

	if (pIE == NULL)
		return;

	if (phepriv->he_option == _FALSE)
		return;

	psta = rtw_get_stainfo(pstapriv, cur_network->MacAddress);
	if (psta == NULL)
		return;
	if (psta->phl_sta == NULL)
		return;

	phl_sta = psta->phl_sta;

	/* HE MAC Caps */
	HE_mac_caps_handler(padapter, phl_sta, ele_start);
	ele_start += HE_CAP_ELE_MAC_CAP_LEN;

	/* HE PHY Caps */
	HE_phy_caps_handler(padapter, phl_sta, ele_start, &supp_mcs_len);
	ele_start += HE_CAP_ELE_PHY_CAP_LEN;

	/* HE Supp MCS Set */
	HE_supp_mcs_handler(padapter, phl_sta, ele_start, supp_mcs_len);
	ele_start += supp_mcs_len;

	/* HE PPE Thresholds */
	HE_ppe_thre_handler(padapter, phl_sta, ele_start);

	pmlmeinfo->HE_enable = 1;
}

void HE_operation_handler(_adapter *padapter, PNDIS_802_11_VARIABLE_IEs pIE)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct sta_priv 		*pstapriv = &padapter->stapriv;
	struct he_priv		*phepriv = &pmlmepriv->hepriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX 		*cur_network = &(pmlmeinfo->network);
	struct sta_info 		*psta = NULL;
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	u8 *ele_start = (&(pIE->data[0]) + 1);

	if (pIE == NULL)
		return;

	if (phepriv->he_option == _FALSE)
		return;

	psta = rtw_get_stainfo(pstapriv, cur_network->MacAddress);
	if (psta == NULL)
		return;
	if (psta->phl_sta == NULL)
		return;

	phl_sta = psta->phl_sta;

	phl_sta->asoc_cap.er_su = !GET_HE_OP_PARA_ER_SU_DISABLE(ele_start);
	phl_sta->asoc_cap.bsscolor = GET_HE_OP_BSS_COLOR_INFO_BSS_COLOR(ele_start);
	phl_sta->tf_trs = _TRUE;
}

void HE_mu_edca_handler(_adapter *padapter, PNDIS_802_11_VARIABLE_IEs pIE, u8 first)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct sta_priv *pstapriv = &padapter->stapriv;
	struct he_priv *phepriv = &pmlmepriv->hepriv;
	struct mlme_ext_priv *pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info *pmlmeinfo = &(pmlmeext->mlmext_info);
	WLAN_BSSID_EX *cur_network = &(pmlmeinfo->network);
	struct sta_info *psta = NULL;
	struct rtw_phl_stainfo_t *phl_sta = NULL;
	u8 *ele_start = (&(pIE->data[0]) + 1);
	struct dvobj_priv *d;
	void *phl;
	struct rtw_mu_edca_param edca = {0};
	u8 pre_cnt = 0, cur_cnt = 0;
	u8 i = 0;

	if (pIE == NULL)
		return;

	if (phepriv->he_option == _FALSE)
		return;

	d = adapter_to_dvobj(padapter);
	phl = GET_PHL_INFO(d);
	if (!phl)
		return;

	psta = rtw_get_stainfo(pstapriv, cur_network->MacAddress);
	if (psta == NULL)
		return;
	if (psta->phl_sta == NULL)
		return;

	phl_sta = psta->phl_sta;

	pre_cnt = phepriv->pre_he_muedca_cnt;
	cur_cnt = GET_HE_MU_EDCA_QOS_INFO_UPDATE_CNT(ele_start);
	if (cur_cnt > pre_cnt || first == _TRUE) {
		phepriv->pre_he_muedca_cnt = cur_cnt;
		phl_sta->asoc_cap.mu_edca[0].ac =
			GET_HE_MU_EDCA_BE_ACI(ele_start);
		phl_sta->asoc_cap.mu_edca[0].aifsn =
			GET_HE_MU_EDCA_BE_AIFSN(ele_start);
		phl_sta->asoc_cap.mu_edca[0].cw =
			GET_HE_MU_EDCA_BE_ECW_MIN_MAX(ele_start);
		phl_sta->asoc_cap.mu_edca[0].timer =
			GET_He_MU_EDCA_BE_TIMER(ele_start);
		phl_sta->asoc_cap.mu_edca[1].ac =
			GET_HE_MU_EDCA_BK_ACI(ele_start);
		phl_sta->asoc_cap.mu_edca[1].aifsn =
			GET_HE_MU_EDCA_BK_AIFSN(ele_start);
		phl_sta->asoc_cap.mu_edca[1].cw =
			GET_HE_MU_EDCA_BK_ECW_MIN_MAX(ele_start);
		phl_sta->asoc_cap.mu_edca[1].timer =
			GET_He_MU_EDCA_BK_TIMER(ele_start);
		phl_sta->asoc_cap.mu_edca[2].ac =
			GET_HE_MU_EDCA_VI_ACI(ele_start);
		phl_sta->asoc_cap.mu_edca[2].aifsn =
			GET_HE_MU_EDCA_VI_AIFSN(ele_start);
		phl_sta->asoc_cap.mu_edca[2].cw =
			GET_HE_MU_EDCA_VI_ECW_MIN_MAX(ele_start);
		phl_sta->asoc_cap.mu_edca[2].timer =
			GET_He_MU_EDCA_VI_TIMER(ele_start);
		phl_sta->asoc_cap.mu_edca[3].ac =
			GET_HE_MU_EDCA_VO_ACI(ele_start);
		phl_sta->asoc_cap.mu_edca[3].aifsn =
			GET_HE_MU_EDCA_VO_AIFSN(ele_start);
		phl_sta->asoc_cap.mu_edca[3].cw =
			GET_HE_MU_EDCA_VO_ECW_MIN_MAX(ele_start);
		phl_sta->asoc_cap.mu_edca[3].timer =
			GET_He_MU_EDCA_VO_TIMER(ele_start);
		for (i = 0; i < 4; i++) {
#ifdef CONFIG_CMD_DISP
			rtw_phl_cmd_wrole_change(phl,
						 padapter->phl_role,
						 WR_CHG_MU_EDCA_PARAM,
						 (u8 *)&phl_sta->asoc_cap.mu_edca[i],
						 sizeof(struct rtw_mu_edca_param),
						 PHL_CMD_NO_WAIT,
						 0);
#endif
			RTW_INFO("%s, Update HE MU EDCA AC(%d) aifsn(%d) cw(0x%x) timer(0x%x)\n",
					__func__,
					phl_sta->asoc_cap.mu_edca[i].ac,
					phl_sta->asoc_cap.mu_edca[i].aifsn,
					phl_sta->asoc_cap.mu_edca[i].cw,
					phl_sta->asoc_cap.mu_edca[i].timer);
		}

		if (first) {
#ifdef CONFIG_CMD_DISP
			rtw_phl_cmd_wrole_change(phl,
						 padapter->phl_role,
						 WR_CHG_MU_EDCA_CFG,
						 (u8 *)&first,
						 sizeof(first),
						 PHL_CMD_NO_WAIT,
						 0);
#else
			;
#endif
		}
	}
}

static u8 *rtw_get_he_muedca_ie(const u8 *in_ie, int in_len)
{
	uint cnt;
	const u8 *mudeca_ie_ptr = NULL;
	u8 eid;

	if (!in_ie || in_len < 0) {
		rtw_warn_on(1);
		return (u8 *)mudeca_ie_ptr;
	}

	if (in_len <= 0)
		return (u8 *)mudeca_ie_ptr;

	cnt = 0;

	while (cnt + 1 + 4 < in_len) {
		eid = in_ie[cnt];

		if (cnt + 1 + 4 >= MAX_IE_SZ) {
			rtw_warn_on(1);
			return NULL;
		}

		if (eid == WLAN_EID_EXTENSION && in_ie[cnt + 2] == WLAN_EID_EXTENSION_HE_MU_EDCA) {
			mudeca_ie_ptr = in_ie + cnt;
			break;
		} else {
			cnt += in_ie[cnt + 1] + 2;
		}

	}

	return (u8 *)mudeca_ie_ptr;
}

void process_he_muedca_ie(_adapter *padapter, u8 *IEs, u32 IELength)
{
	u8 *ies;
	u32 ies_len;
	u8 *muedca_ie;

	if (IELength <= _BEACON_IE_OFFSET_)
		return;

	ies = IEs + _BEACON_IE_OFFSET_;
	ies_len = IELength - _BEACON_IE_OFFSET_;

	muedca_ie = rtw_get_he_muedca_ie(ies, ies_len);

	if (muedca_ie)
		HE_mu_edca_handler(padapter, (struct _NDIS_802_11_VARIABLE_IEs *)muedca_ie, _FALSE);

}

static int rtw_build_he_mac_caps(struct protocol_cap_t *proto_cap, u8 *pbuf)
{
	/* Set HE MAC Capabilities Information */

	int info_len = HE_CAP_ELE_MAC_CAP_LEN;

	if (proto_cap->htc_rx)
		SET_HE_MAC_CAP_HTC_HE_SUPPORT(pbuf, 1);

	if (proto_cap->twt & BIT(0))
		SET_HE_MAC_CAP_TWT_REQUESTER_SUPPORT(pbuf, 1);

	if (proto_cap->twt & BIT(1))
		SET_HE_MAC_CAP_TWT_RESPONDER_SUPPORT(pbuf, 1);

	if (proto_cap->trig_padding)
		SET_HE_MAC_CAP_TRI_FRAME_PADDING_DUR(pbuf,
			proto_cap->trig_padding);

	if (proto_cap->all_ack)
		SET_HE_MAC_CAP_ALL_ACK_SUPPORT(pbuf, 1);

	if (proto_cap->htc_rx && (proto_cap->a_ctrl & BIT(0)))
		SET_HE_MAC_CAP_TRS_SUPPORT(pbuf, 1);

	if (proto_cap->a_ctrl & BIT(1))
		SET_HE_MAC_CAP_BRS_SUPPORT(pbuf, 1);

	if (proto_cap->twt & BIT(2))
		SET_HE_MAC_CAP_BC_TWT_SUPPORT(pbuf, 1);

	if (proto_cap->htc_rx && (proto_cap->a_ctrl & BIT(2)))
		SET_HE_MAC_CAP_OM_CTRL_SUPPORT(pbuf, 1);

	SET_HE_MAC_CAP_MAX_AMPDU_LEN_EXP_EXT(pbuf, 2);

	if (proto_cap->twt & BIT(3))
		SET_HE_MAC_CAP_FLEX_TWT_SCHED_SUPPORT(pbuf, 1);

	if (proto_cap->twt & BIT(4))
		SET_HE_MAC_CAP_PSR_RESPONDER(pbuf, 1);

	if (proto_cap->ops)
		SET_HE_MAC_CAP_OPS_SUPPORT(pbuf, 1);

	if (proto_cap->amsdu_in_ampdu)
		SET_HE_MAC_CAP_AMSDU_NOT_UNDER_BA_IN_ACK_EN_AMPDU(pbuf, 1);

	if (proto_cap->twt & BIT(5))
		SET_HE_MAC_CAP_HE_SUB_CH_SELECTIVE_TX(pbuf, 1);

	if (proto_cap->ht_vht_trig_rx)
		SET_HE_MAC_CAP_HT_VHT_TRIG_FRAME_RX(pbuf, 1);

	return info_len;
}

static int rtw_build_he_phy_caps(struct protocol_cap_t *proto_cap, u8 *pbuf)
{
	/* struct rtw_chan_def *chan_def = &(wrole->chandef); */
	/* Set HE PHY Capabilities Information */

	int info_len = HE_CAP_ELE_PHY_CAP_LEN;

#if 1
	SET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(pbuf, (BIT(0) | BIT(1)));
#else
	u8 bw_cap = 0;

	if (phy_cap->bw_sup & BW_CAP_40M)
		bw_cap |= BIT(0);
	if (phy_cap->bw_sup & BW_CAP_80M)
		bw_cap |= BIT(1);

	if (chan_def->band == BAND_ON_24G) {
		if (chan_def->bw == CHANNEL_WIDTH_40)
			SET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(pbuf, BIT(0));
	} else if (chan_def->band == BAND_ON_5G) {
		if (chan_def->bw == CHANNEL_WIDTH_80)
			SET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(pbuf, BIT(1));
		else if (chan_def->bw == CHANNEL_WIDTH_160)
			SET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(pbuf, (BIT(1) | BIT(2)));
		else if (chan_def->bw == CHANNEL_WIDTH_80_80)
			SET_HE_PHY_CAP_SUPPORT_CHAN_WIDTH_SET(pbuf, (BIT(1) | BIT(3)));
	}
#endif

	SET_HE_PHY_CAP_DEVICE_CLASS(pbuf, HE_DEV_CLASS_A);

	if (proto_cap->he_ldpc)
		SET_HE_PHY_CAP_LDPC_IN_PAYLOAD(pbuf, 1);

	SET_HE_PHY_CAP_SU_PPDU_1X_LTF_0_POINT_8_GI(pbuf, 1);

	if (proto_cap->he_rx_ndp_4x32) {
		SET_HE_PHY_CAP_NDP_4X_LTF_3_POINT_2_GI(pbuf, 1);
		RTW_INFO("NDP_4x32 is set.\n");;
	}

	if (proto_cap->stbc_tx)
		SET_HE_PHY_CAP_STBC_TX_LESS_THAN_80MHZ(pbuf, 1);

	if (proto_cap->stbc_he_rx)
		SET_HE_PHY_CAP_STBC_RX_LESS_THAN_80MHZ(pbuf, 1);

	if (proto_cap->doppler_tx)
		SET_HE_PHY_CAP_DOPPLER_TX(pbuf, 1);

	if (proto_cap->doppler_rx)
		SET_HE_PHY_CAP_DOPPLER_RX(pbuf, 1);

	if (proto_cap->dcm_max_const_tx)
		SET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_TX(pbuf,
			proto_cap->dcm_max_const_tx);

	if (proto_cap->dcm_max_nss_tx)
		SET_HE_PHY_CAP_DCM_MAX_NSS_TX(pbuf, 1);

	if (proto_cap->dcm_max_const_rx)
		SET_HE_PHY_CAP_DCM_MAX_CONSTELLATION_RX(pbuf,
			proto_cap->dcm_max_const_rx);

	if (proto_cap->dcm_max_nss_rx)
		SET_HE_PHY_CAP_DCM_MAX_NSS_RX(pbuf, 1);

	if (proto_cap->partial_bw_su_in_mu)
		SET_HE_PHY_CAP_RX_PARTIAL_BW_SU_IN_20MHZ_MUPPDU(pbuf, 1);

	if (proto_cap->he_su_bfmr)
		SET_HE_PHY_CAP_SU_BFER(pbuf, 1);

	if (proto_cap->he_su_bfme)
		SET_HE_PHY_CAP_SU_BFEE(pbuf, 1);

	if (proto_cap->he_mu_bfmr)
		SET_HE_PHY_CAP_MU_BFER(pbuf, 1);

	if (proto_cap->bfme_sts)
		SET_HE_PHY_CAP_BFEE_STS_LESS_THAN_80MHZ(pbuf,
			proto_cap->bfme_sts);

	if (proto_cap->bfme_sts_greater_80mhz)
		SET_HE_PHY_CAP_BFEE_STS_GREATER_THAN_80MHZ(pbuf,
			proto_cap->bfme_sts_greater_80mhz);

	if (proto_cap->num_snd_dim)
		SET_HE_PHY_CAP_NUM_SND_DIMEN_LESS_THAN_80MHZ(pbuf,
			proto_cap->num_snd_dim);

	if (proto_cap->num_snd_dim_greater_80mhz)
		SET_HE_PHY_CAP_NUM_SND_DIMEN_GREATER_THAN_80MHZ(pbuf,
			proto_cap->num_snd_dim_greater_80mhz);

	if (proto_cap->ng_16_su_fb)
		SET_HE_PHY_CAP_NG_16_SU_FEEDBACK(pbuf, 1);

	if (proto_cap->ng_16_mu_fb)
		SET_HE_PHY_CAP_NG_16_MU_FEEDBACK(pbuf, 1);

	if (proto_cap->cb_sz_su_fb)
		SET_HE_PHY_CAP_CODEBOOK_4_2_SU_FEEDBACK(pbuf, 1);

	if (proto_cap->cb_sz_mu_fb)
		SET_HE_PHY_CAP_CODEBOOK_7_5_MU_FEEDBACK(pbuf, 1);

	if (proto_cap->trig_su_bfm_fb)
		SET_HE_PHY_CAP_TRIG_SUBF_FEEDBACK(pbuf, 1);

	if (proto_cap->trig_mu_bfm_fb)
		SET_HE_PHY_CAP_TRIG_MUBF_PARTIAL_BW_FEEDBACK(pbuf, 1);

	if (proto_cap->trig_cqi_fb)
		SET_HE_PHY_CAP_TRIG_CQI_FEEDBACK(pbuf, 1);

	if (proto_cap->partial_bw_su_er)
		SET_HE_PHY_CAP_PARTIAL_BW_EXT_RANGE(pbuf, 1);

	if (proto_cap->pwr_bst_factor)
		SET_HE_PHY_CAP_PWR_BOOST_FACTOR_SUPPORT(pbuf, 1);

	SET_HE_PHY_CAP_SU_MU_PPDU_4X_LTF_0_POINT_8_GI(pbuf, 1);

	if (proto_cap->max_nc)
		SET_HE_PHY_CAP_MAX_NC(pbuf, proto_cap->max_nc);

	if (proto_cap->stbc_tx_greater_80mhz)
		SET_HE_PHY_CAP_STBC_TX_GREATER_THAN_80MHZ(pbuf, 1);

	if (proto_cap->stbc_rx_greater_80mhz)
		SET_HE_PHY_CAP_STBC_RX_GREATER_THAN_80MHZ(pbuf, 1);

	SET_HE_PHY_CAP_ERSU_PPDU_4X_LTF_0_POINT_8_GI(pbuf, 1);
	SET_HE_PHY_CAP_ERSU_PPDU_1X_LTF_0_POINT_8_GI(pbuf, 1);

	if (proto_cap->dcm_max_ru)
		SET_HE_PHY_CAP_DCM_MAX_RU(pbuf, proto_cap->dcm_max_ru);

	if (proto_cap->long_sigb_symbol)
		SET_HE_PHY_CAP_LONGER_THAN_16_HESIGB_OFDM_SYM(pbuf, 1);

	if (proto_cap->non_trig_cqi_fb)
		SET_HE_PHY_CAP_NON_TRIGGER_CQI_FEEDBACK(pbuf, 1);

	if (proto_cap->tx_1024q_ru)
		SET_HE_PHY_CAP_TX_1024_QAM_LESS_THAN_242_TONE_RU(pbuf, 1);

	if (proto_cap->rx_1024q_ru)
		SET_HE_PHY_CAP_RX_1024_QAM_LESS_THAN_242_TONE_RU(pbuf, 1);

	if (proto_cap->fbw_su_using_mu_cmprs_sigb)
		SET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_CMP_SIGB(pbuf, 1);

	if (proto_cap->fbw_su_using_mu_non_cmprs_sigb)
		SET_HE_PHY_CAP_RX_FULLBW_SU_USE_MUPPDU_NONCMP_SIGB(pbuf, 1);

	if (proto_cap->pkt_padding)
		SET_HE_PHY_CAP_NOMINAL_PACKET_PADDING(pbuf,
			proto_cap->pkt_padding);

	return info_len;
}

static int rtw_build_he_supp_mcs(struct protocol_cap_t *proto_cap, u8 *pbuf)
{

	 /* struct rtw_chan_def *chan_def = &(wrole->chandef); */

	/* Set HE Supported MCS and NSS Set */

	int info_len = 4;

	_rtw_memset(pbuf, HE_MSC_NOT_SUPP_BYTE, info_len);

	_rtw_memcpy(pbuf, proto_cap->he_rx_mcs, HE_CAP_ELE_SUPP_MCS_LEN_RX_80M);

	_rtw_memcpy(pbuf + 2, proto_cap->he_tx_mcs, HE_CAP_ELE_SUPP_MCS_LEN_TX_80M);

	return info_len;
}

static int rtw_build_he_ppe_thre(struct protocol_cap_t *proto_cap, u8 *pbuf)
{
	/* Set HE PPE Thresholds (optional) */

	int info_len = 0;

	return info_len;
}

u32 rtw_get_dft_he_cap_ie(_adapter *padapter, struct phy_cap_t *phy_cap,
		struct protocol_cap_t *proto_cap, u8 *pbuf)
{
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct he_priv	*phepriv = &pmlmepriv->hepriv;
	u32 he_cap_total_len = 0, len = 0;
	u8* pcap_start = phepriv->he_cap;
	u8* pcap = pcap_start;

	_rtw_memset(pcap, 0, HE_CAP_ELE_MAX_LEN);

	/* Ele ID Extension */
	*pcap++ = WLAN_EID_EXTENSION_HE_CAPABILITY;

	/* HE MAC Caps */
	pcap += rtw_build_he_mac_caps(proto_cap, pcap);

	/* HE PHY Caps */
	pcap += rtw_build_he_phy_caps(proto_cap, pcap);

	/* HE Supported MCS and NSS Set */
	pcap += rtw_build_he_supp_mcs(proto_cap, pcap);

	/* HE PPE Thresholds (optional) */
	pcap += rtw_build_he_ppe_thre(proto_cap, pcap);

	he_cap_total_len = (pcap - pcap_start);

	pbuf = rtw_set_ie(pbuf, WLAN_EID_EXTENSION, he_cap_total_len, pcap_start, &len);

	return len;
}

u32 rtw_build_he_cap_ie(_adapter *padapter, u8 *pbuf)
{
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *proto_cap = &(wrole->proto_role_cap);
	struct mlme_priv *pmlmepriv = &padapter->mlmepriv;
	struct he_priv	*phepriv = &pmlmepriv->hepriv;
	u32 he_cap_total_len = 0, len = 0;
	u8* pcap_start = phepriv->he_cap;
	u8* pcap = pcap_start;

	_rtw_memset(pcap, 0, HE_CAP_ELE_MAX_LEN);

	/* Ele ID Extension */
	*pcap++ = WLAN_EID_EXTENSION_HE_CAPABILITY;

	/* HE MAC Caps */
	pcap += rtw_build_he_mac_caps(proto_cap, pcap);

	/* HE PHY Caps */
	pcap += rtw_build_he_phy_caps(proto_cap, pcap);

	/* HE Supported MCS and NSS Set */
	pcap += rtw_build_he_supp_mcs(proto_cap, pcap);

	/* HE PPE Thresholds (optional) */
	pcap += rtw_build_he_ppe_thre(proto_cap, pcap);

	he_cap_total_len = (pcap - pcap_start);

	pbuf = rtw_set_ie(pbuf, WLAN_EID_EXTENSION, he_cap_total_len, pcap_start, &len);

	return len;
}

u32 rtw_restructure_he_ie(_adapter *padapter, u8 *in_ie, u8 *out_ie, uint in_len, uint *pout_len)
{
	struct mlme_priv	*pmlmepriv = &padapter->mlmepriv;
	struct he_priv	*phepriv = &pmlmepriv->hepriv;
	u32	ielen;
	u8 *out_he_op_ie, *he_cap_ie, *he_op_ie;
	u8 he_cap_eid_ext = WLAN_EID_EXTENSION_HE_CAPABILITY;
	u8 he_op_eid_ext = WLAN_EID_EXTENSION_HE_OPERATION;

	rtw_he_use_default_setting(padapter);

	he_cap_ie = rtw_get_ie_ex(in_ie + 12, in_len - 12, WLAN_EID_EXTENSION, &he_cap_eid_ext, 1, NULL, &ielen);
	if (!he_cap_ie || (ielen > (HE_CAP_ELE_MAX_LEN + 2)))
		goto exit;
	he_op_ie = rtw_get_ie_ex(in_ie + 12, in_len - 12, WLAN_EID_EXTENSION, &he_op_eid_ext, 1, NULL, &ielen);
	if (!he_op_ie || (ielen > (HE_OPER_ELE_MAX_LEN + 2)))
		goto exit;

	*pout_len += rtw_build_he_cap_ie(padapter, out_ie + *pout_len);

	phepriv->he_option = _TRUE;

exit:
	return phepriv->he_option;
}

static int rtw_build_he_oper_params(_adapter *padapter, u8 *pbuf)
{
	/* Set HE Operation Parameters */

	int info_len = HE_OPER_PARAMS_LEN;

	SET_HE_OP_PARA_DEFAULT_PE_DUR(pbuf, 0x4);

	return info_len;
}

static int rtw_build_he_oper_bss_color_info(_adapter *padapter, u8 *pbuf)
{
	/* Set BSS Color Information */
	int info_len = HE_OPER_BSS_COLOR_INFO_LEN;
	struct rtw_wifi_role_t *wrole = padapter->phl_role;
	struct protocol_cap_t *proto_cap = &(wrole->proto_role_cap);

	SET_HE_OP_BSS_COLOR_INFO_BSS_COLOR(pbuf, proto_cap->bsscolor);

	return info_len;
}

static int rtw_build_he_oper_basic_mcs_set(_adapter *padapter, u8 *pbuf)
{
	/* Set Basic HE-MCS and NSS Set */

	int info_len = HE_OPER_BASIC_MCS_LEN;

	_rtw_memset(pbuf, HE_MSC_NOT_SUPP_BYTE, info_len);

	SET_HE_OP_BASIC_MCS_1SS(pbuf, HE_MCS_SUPP_MSC0_TO_MSC11);
	SET_HE_OP_BASIC_MCS_2SS(pbuf, HE_MCS_SUPP_MSC0_TO_MSC11);

	return info_len;
}

static int rtw_build_vht_oper_info(_adapter *padapter, u8 *pbuf)
{
	/* Set VHT Operation Information (optional) */

	int info_len = 0;

	return info_len;
}

static int rtw_build_max_cohost_bssid_ind(_adapter *padapter, u8 *pbuf)
{
	/* Set Max Co-Hosted BSSID Indicator (optional) */

	int info_len = 0;

	return info_len;
}

static int rtw_build_6g_oper_info(_adapter *padapter, u8 *pbuf)
{
	/* Set 6GHz Operation Information (optional) */

	int info_len = 0;

	return info_len;
}

static u32	rtw_build_he_operation_ie(_adapter *padapter, u8 *pbuf)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct he_priv		*phepriv = &pmlmepriv->hepriv;
	u32 he_oper_total_len = 0, len = 0;
	u8* poper_start = phepriv->he_op;
	u8* poper = poper_start;

	_rtw_memset(poper, 0, HE_OPER_ELE_MAX_LEN);

	/* Ele ID Extension */
	*poper++ = WLAN_EID_EXTENSION_HE_OPERATION;

	/* HE Oper Params */
	poper += rtw_build_he_oper_params(padapter, poper);

	/* BSS Color Info */
	poper += rtw_build_he_oper_bss_color_info(padapter, poper);

	/* Basic MCS and NSS Set */
	poper += rtw_build_he_oper_basic_mcs_set(padapter, poper);

	/* VHT Oper Info */
	poper += rtw_build_vht_oper_info(padapter, poper);

	/* Max Co-Hosted BSSID Indicator */
	poper += rtw_build_max_cohost_bssid_ind(padapter, poper);

	/* 6G Oper Info */
	poper += rtw_build_6g_oper_info(padapter, poper);

	he_oper_total_len = (poper - poper_start);

	pbuf = rtw_set_ie(pbuf, WLAN_EID_EXTENSION, he_oper_total_len, poper_start, &len);

	return len;
}

void HEOnAssocRsp(_adapter *padapter)
{
	struct mlme_priv		*pmlmepriv = &padapter->mlmepriv;
	struct vht_priv		*pvhtpriv = &pmlmepriv->vhtpriv;
	struct mlme_ext_priv	*pmlmeext = &padapter->mlmeextpriv;
	struct mlme_ext_info	*pmlmeinfo = &(pmlmeext->mlmext_info);
	u8	ht_AMPDU_len;

	if (!pmlmeinfo->VHT_enable)
		return;

	if (!pmlmeinfo->HE_enable)
		return;

	RTW_INFO("%s\n", __FUNCTION__);

	/* AMPDU related settings here ? */
}

void rtw_he_ies_attach(_adapter *padapter, WLAN_BSSID_EX *pnetwork)
{
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u8 he_cap_eid_ext = WLAN_EID_EXTENSION_HE_CAPABILITY;
	u8 cap_len, operation_len;
	uint len = 0;
	sint ie_len = 0;
	u8 *p = NULL;

	p = rtw_get_ie_ex(pnetwork->IEs + _BEACON_IE_OFFSET_, pnetwork->IELength - _BEACON_IE_OFFSET_,
		WLAN_EID_EXTENSION, &he_cap_eid_ext, 1, NULL, &ie_len);
	if (p && ie_len > 0)
		return;

	rtw_he_use_default_setting(padapter);

	cap_len = rtw_build_he_cap_ie(padapter, pnetwork->IEs + pnetwork->IELength);
	pnetwork->IELength += cap_len;

	operation_len = rtw_build_he_operation_ie(padapter, pnetwork->IEs + pnetwork->IELength);
	pnetwork->IELength += operation_len;

	pmlmepriv->hepriv.he_option = _TRUE;
}

void rtw_he_ies_detach(_adapter *padapter, WLAN_BSSID_EX *pnetwork)
{
	struct mlme_priv *pmlmepriv = &(padapter->mlmepriv);
	u8 he_cap_eid_ext = WLAN_EID_EXTENSION_HE_CAPABILITY;
	u8 he_op_eid_ext = WLAN_EID_EXTENSION_HE_OPERATION;

	rtw_remove_bcn_ie_ex(padapter, pnetwork, WLAN_EID_EXTENSION, &he_cap_eid_ext, 1);
	rtw_remove_bcn_ie_ex(padapter, pnetwork, WLAN_EID_EXTENSION, &he_op_eid_ext, 1);

	pmlmepriv->hepriv.he_option = _FALSE;
}

u8 rtw_he_htc_en(_adapter *padapter, struct sta_info *psta)
{
	return 1;
}

void rtw_he_fill_htc(_adapter *padapter, struct pkt_attrib *pattrib, u32 *phtc_buf)
{
	SET_HE_VAR_HTC(phtc_buf);
	SET_HE_VAR_HTC_CID_CAS(phtc_buf);

	/* CONFIG_80211AX_HE_TODO */
}

#endif /* CONFIG_80211AX_HE */

