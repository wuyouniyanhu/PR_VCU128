--Copyright 1986-2019 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2019.1 (win64) Build 2552052 Fri May 24 14:49:42 MDT 2019
--Date        : Fri Jul 31 18:50:19 2020
--Host        : DESKTOP-0HGFOUP running 64-bit major release  (build 9200)
--Command     : generate_target design_1_wrapper.bd
--Design      : design_1_wrapper
--Purpose     : IP block netlist
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
library UNISIM;
use UNISIM.VCOMPONENTS.ALL;
entity design_1_wrapper is
  port (
    ICAP_0_avail : in STD_LOGIC;
    ICAP_0_csib : out STD_LOGIC;
    ICAP_0_i : out STD_LOGIC_VECTOR ( 31 downto 0 );
    ICAP_0_o : in STD_LOGIC_VECTOR ( 31 downto 0 );
    ICAP_0_rdwrb : out STD_LOGIC;
    addn_ui_clkout3_0 : out STD_LOGIC;
    ddr4_sdram_act_n : out STD_LOGIC;
    ddr4_sdram_adr : out STD_LOGIC_VECTOR ( 16 downto 0 );
    ddr4_sdram_ba : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ddr4_sdram_bg : out STD_LOGIC;
    ddr4_sdram_ck_c : out STD_LOGIC;
    ddr4_sdram_ck_t : out STD_LOGIC;
    ddr4_sdram_cke : out STD_LOGIC;
    ddr4_sdram_cs_n : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ddr4_sdram_dm_n : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_dq : inout STD_LOGIC_VECTOR ( 71 downto 0 );
    ddr4_sdram_dqs_c : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_dqs_t : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_odt : out STD_LOGIC;
    ddr4_sdram_reset_n : out STD_LOGIC;
    default_100mhz_clk_clk_n : in STD_LOGIC;
    default_100mhz_clk_clk_p : in STD_LOGIC;
    dummy_port_in : in STD_LOGIC;
    mdio_mdc_mdc : out STD_LOGIC;
    mdio_mdc_mdio_io : inout STD_LOGIC;
    reset : in STD_LOGIC;
    rs232_uart_0_rxd : in STD_LOGIC;
    rs232_uart_0_txd : out STD_LOGIC;
    sgmii_lvds_rxn : in STD_LOGIC;
    sgmii_lvds_rxp : in STD_LOGIC;
    sgmii_lvds_txn : out STD_LOGIC;
    sgmii_lvds_txp : out STD_LOGIC;
    sgmii_phyclk_clk_n : in STD_LOGIC;
    sgmii_phyclk_clk_p : in STD_LOGIC
  );
end design_1_wrapper;

architecture STRUCTURE of design_1_wrapper is
  component design_1 is
  port (
    addn_ui_clkout3_0 : out STD_LOGIC;
    dummy_port_in : in STD_LOGIC;
    reset : in STD_LOGIC;
    mdio_mdc_mdc : out STD_LOGIC;
    mdio_mdc_mdio_i : in STD_LOGIC;
    mdio_mdc_mdio_o : out STD_LOGIC;
    mdio_mdc_mdio_t : out STD_LOGIC;
    sgmii_lvds_rxn : in STD_LOGIC;
    sgmii_lvds_rxp : in STD_LOGIC;
    sgmii_lvds_txn : out STD_LOGIC;
    sgmii_lvds_txp : out STD_LOGIC;
    ICAP_0_avail : in STD_LOGIC;
    ICAP_0_csib : out STD_LOGIC;
    ICAP_0_i : out STD_LOGIC_VECTOR ( 31 downto 0 );
    ICAP_0_o : in STD_LOGIC_VECTOR ( 31 downto 0 );
    ICAP_0_rdwrb : out STD_LOGIC;
    rs232_uart_0_rxd : in STD_LOGIC;
    rs232_uart_0_txd : out STD_LOGIC;
    ddr4_sdram_act_n : out STD_LOGIC;
    ddr4_sdram_adr : out STD_LOGIC_VECTOR ( 16 downto 0 );
    ddr4_sdram_ba : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ddr4_sdram_bg : out STD_LOGIC;
    ddr4_sdram_ck_c : out STD_LOGIC;
    ddr4_sdram_ck_t : out STD_LOGIC;
    ddr4_sdram_cke : out STD_LOGIC;
    ddr4_sdram_cs_n : out STD_LOGIC_VECTOR ( 1 downto 0 );
    ddr4_sdram_dm_n : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_dq : inout STD_LOGIC_VECTOR ( 71 downto 0 );
    ddr4_sdram_dqs_c : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_dqs_t : inout STD_LOGIC_VECTOR ( 8 downto 0 );
    ddr4_sdram_odt : out STD_LOGIC;
    ddr4_sdram_reset_n : out STD_LOGIC;
    default_100mhz_clk_clk_n : in STD_LOGIC;
    default_100mhz_clk_clk_p : in STD_LOGIC;
    sgmii_phyclk_clk_n : in STD_LOGIC;
    sgmii_phyclk_clk_p : in STD_LOGIC
  );
  end component design_1;
  component IOBUF is
  port (
    I : in STD_LOGIC;
    O : out STD_LOGIC;
    T : in STD_LOGIC;
    IO : inout STD_LOGIC
  );
  end component IOBUF;
  signal mdio_mdc_mdio_i : STD_LOGIC;
  signal mdio_mdc_mdio_o : STD_LOGIC;
  signal mdio_mdc_mdio_t : STD_LOGIC;
begin
design_1_i: component design_1
     port map (
      ICAP_0_avail => ICAP_0_avail,
      ICAP_0_csib => ICAP_0_csib,
      ICAP_0_i(31 downto 0) => ICAP_0_i(31 downto 0),
      ICAP_0_o(31 downto 0) => ICAP_0_o(31 downto 0),
      ICAP_0_rdwrb => ICAP_0_rdwrb,
      addn_ui_clkout3_0 => addn_ui_clkout3_0,
      ddr4_sdram_act_n => ddr4_sdram_act_n,
      ddr4_sdram_adr(16 downto 0) => ddr4_sdram_adr(16 downto 0),
      ddr4_sdram_ba(1 downto 0) => ddr4_sdram_ba(1 downto 0),
      ddr4_sdram_bg => ddr4_sdram_bg,
      ddr4_sdram_ck_c => ddr4_sdram_ck_c,
      ddr4_sdram_ck_t => ddr4_sdram_ck_t,
      ddr4_sdram_cke => ddr4_sdram_cke,
      ddr4_sdram_cs_n(1 downto 0) => ddr4_sdram_cs_n(1 downto 0),
      ddr4_sdram_dm_n(8 downto 0) => ddr4_sdram_dm_n(8 downto 0),
      ddr4_sdram_dq(71 downto 0) => ddr4_sdram_dq(71 downto 0),
      ddr4_sdram_dqs_c(8 downto 0) => ddr4_sdram_dqs_c(8 downto 0),
      ddr4_sdram_dqs_t(8 downto 0) => ddr4_sdram_dqs_t(8 downto 0),
      ddr4_sdram_odt => ddr4_sdram_odt,
      ddr4_sdram_reset_n => ddr4_sdram_reset_n,
      default_100mhz_clk_clk_n => default_100mhz_clk_clk_n,
      default_100mhz_clk_clk_p => default_100mhz_clk_clk_p,
      dummy_port_in => dummy_port_in,
      mdio_mdc_mdc => mdio_mdc_mdc,
      mdio_mdc_mdio_i => mdio_mdc_mdio_i,
      mdio_mdc_mdio_o => mdio_mdc_mdio_o,
      mdio_mdc_mdio_t => mdio_mdc_mdio_t,
      reset => reset,
      rs232_uart_0_rxd => rs232_uart_0_rxd,
      rs232_uart_0_txd => rs232_uart_0_txd,
      sgmii_lvds_rxn => sgmii_lvds_rxn,
      sgmii_lvds_rxp => sgmii_lvds_rxp,
      sgmii_lvds_txn => sgmii_lvds_txn,
      sgmii_lvds_txp => sgmii_lvds_txp,
      sgmii_phyclk_clk_n => sgmii_phyclk_clk_n,
      sgmii_phyclk_clk_p => sgmii_phyclk_clk_p
    );
mdio_mdc_mdio_iobuf: component IOBUF
     port map (
      I => mdio_mdc_mdio_o,
      IO => mdio_mdc_mdio_io,
      O => mdio_mdc_mdio_i,
      T => mdio_mdc_mdio_t
    );
end STRUCTURE;
