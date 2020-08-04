-- This is the tutorial design controlled by a prototype Partial Reconfiguration Controller
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;

library UNISIM;
use UNISIM.vcomponents.all;

entity top is
  port(
    -- Clocks
    --
    -- clk_p     : in  std_logic;                     -- 200MHz differential input clock
    -- clk_n     : in  std_logic;                     -- 200MHz differential input clock
    -- reset     : in std_logic;
    count_out : out std_logic_vector(3 downto 0);  -- mapped to general purpose LEDs[4-7]
    shift_out : out std_logic_vector(3 downto 0);  -- mapped to general purpose LEDs[0-3]

    -- -- DDR3 Memory
    -- --
    -- ddr3_sdram_addr : out STD_LOGIC_VECTOR ( 13 downto 0 );
    -- ddr3_sdram_ba : out STD_LOGIC_VECTOR ( 2 downto 0 );
    -- ddr3_sdram_cas_n : out STD_LOGIC;
    -- ddr3_sdram_ck_n : out STD_LOGIC_VECTOR ( 0 to 0 );
    -- ddr3_sdram_ck_p : out STD_LOGIC_VECTOR ( 0 to 0 );
    -- ddr3_sdram_cke : out STD_LOGIC_VECTOR ( 0 to 0 );
    -- ddr3_sdram_cs_n : out STD_LOGIC_VECTOR ( 0 to 0 );
    -- ddr3_sdram_dm : out STD_LOGIC_VECTOR ( 7 downto 0 );
    -- ddr3_sdram_dq : inout STD_LOGIC_VECTOR ( 63 downto 0 );
    -- ddr3_sdram_dqs_n : inout STD_LOGIC_VECTOR ( 7 downto 0 );
    -- ddr3_sdram_dqs_p : inout STD_LOGIC_VECTOR ( 7 downto 0 );
    -- ddr3_sdram_odt : out STD_LOGIC_VECTOR ( 0 to 0 );
    -- ddr3_sdram_ras_n : out STD_LOGIC;
    -- ddr3_sdram_reset_n : out STD_LOGIC;
    -- ddr3_sdram_we_n : out STD_LOGIC;

    -- -- Ethernet
    -- mdio_mdc_mdc : out STD_LOGIC;
    -- mdio_mdc_mdio_io : inout STD_LOGIC;
    -- mii_col : in STD_LOGIC;
    -- mii_crs : in STD_LOGIC;
    -- mii_rst_n : out STD_LOGIC;
    -- mii_rx_clk : in STD_LOGIC;
    -- mii_rx_dv : in STD_LOGIC;
    -- mii_rx_er : in STD_LOGIC;
    -- mii_rxd : in STD_LOGIC_VECTOR ( 3 downto 0 );
    -- mii_tx_clk : in STD_LOGIC;
    -- mii_tx_en : out STD_LOGIC;
    -- mii_txd : out STD_LOGIC_VECTOR ( 3 downto 0 );
   
    -- -- UART
    -- rs232_uart_rxd : in STD_LOGIC;
    -- rs232_uart_txd : out STD_LOGIC

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
end top;

architecture rtl of top is

  component static_bd_wrapper
    port (
      -- ICAP_csib : out STD_LOGIC;
      -- ICAP_rdwrb : out STD_LOGIC;
      -- clk_100 : out STD_LOGIC;
      -- ddr3_sdram_addr : out STD_LOGIC_VECTOR ( 13 downto 0 );
      -- ddr3_sdram_ba : out STD_LOGIC_VECTOR ( 2 downto 0 );
      -- ddr3_sdram_cas_n : out STD_LOGIC;
      -- ddr3_sdram_ck_n : out STD_LOGIC_VECTOR ( 0 to 0 );
      -- ddr3_sdram_ck_p : out STD_LOGIC_VECTOR ( 0 to 0 );
      -- ddr3_sdram_cke : out STD_LOGIC_VECTOR ( 0 to 0 );
      -- ddr3_sdram_cs_n : out STD_LOGIC_VECTOR ( 0 to 0 );
      -- ddr3_sdram_dm : out STD_LOGIC_VECTOR ( 7 downto 0 );
      -- ddr3_sdram_dq : inout STD_LOGIC_VECTOR ( 63 downto 0 );
      -- ddr3_sdram_dqs_n : inout STD_LOGIC_VECTOR ( 7 downto 0 );
      -- ddr3_sdram_dqs_p : inout STD_LOGIC_VECTOR ( 7 downto 0 );
      -- ddr3_sdram_odt : out STD_LOGIC_VECTOR ( 0 to 0 );
      -- ddr3_sdram_ras_n : out STD_LOGIC;
      -- ddr3_sdram_reset_n : out STD_LOGIC;
      -- ddr3_sdram_we_n : out STD_LOGIC;
      -- icap_i : out STD_LOGIC_VECTOR ( 31 downto 0 );
      -- icap_o : in STD_LOGIC_VECTOR ( 31 downto 0 );
      -- mdio_mdc_mdc : out STD_LOGIC;
      -- mdio_mdc_mdio_io : inout STD_LOGIC;
      -- mii_col : in STD_LOGIC;
      -- mii_crs : in STD_LOGIC;
      -- mii_rst_n : out STD_LOGIC;
      -- mii_rx_clk : in STD_LOGIC;
      -- mii_rx_dv : in STD_LOGIC;
      -- mii_rx_er : in STD_LOGIC;
      -- mii_rxd : in STD_LOGIC_VECTOR ( 3 downto 0 );
      -- mii_tx_clk : in STD_LOGIC;
      -- mii_tx_en : out STD_LOGIC;
      -- mii_txd : out STD_LOGIC_VECTOR ( 3 downto 0 );
      -- reset : in STD_LOGIC;
      -- rs232_uart_rxd : in STD_LOGIC;
      -- rs232_uart_txd : out STD_LOGIC;
      -- sys_diff_clock_clk_n : in STD_LOGIC;
      -- sys_diff_clock_clk_p : in STD_LOGIC
      
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
  end component;

  
 
  -- -------------------------------------------------------------------------
  -- System signals
  -- -------------------------------------------------------------------------
  signal addn_ui_clkout3_0   : std_logic;         -- 100 MHz clock

  constant C_ICAP_DATA_WIDTH : integer := 32;

  -- ------------------------------------------------------------------------  
  -- DUT Ports
  -- ------------------------------------------------------------------------

  -- The ICAP interace signals
  signal icap_0_i     : std_logic_vector(C_ICAP_DATA_WIDTH-1 downto 0); 
  signal icap_0_o     : std_logic_vector(C_ICAP_DATA_WIDTH-1 downto 0); 
  signal icap_0_csib  : std_logic;
  signal icap_0_rdwrb : std_logic;

  signal ICAP_0_avail : std_logic;

  signal count_value      : std_logic_vector(34 downto 0);

  component shift
    port(
      en       : in  std_logic;
      clk      : in  std_logic;
      addr     : in  std_logic_vector (11 downto 0);
      data_out : out std_logic_vector (3 downto 0)
      );
  end component;

  component count
    port (
      rst       : in  std_logic;
      clk       : in  std_logic;
      count_out : out std_logic_vector(3 downto 0)
      );
  end component;

  component ila_icap
    port (
      clk          : in  std_logic;
      trig_out     : out std_logic;
      trig_out_ack : in  std_logic;
      probe0       : in  std_logic_vector(31 downto 0);
      probe1       : in  std_logic_vector(3 downto 0);
      probe2       : in  std_logic_vector(0 downto 0);
      probe3       : in  std_logic_vector(0 downto 0)
      );
  end component;

begin

 
  i_static: static_bd_wrapper
    port map (
      -- Clocks and reset
      reset                => reset,
      -- sys_diff_clock_clk_n => clk_n,
      -- sys_diff_clock_clk_p => clk_p,
      default_100mhz_clk_clk_n => default_100mhz_clk_clk_n,
      default_100mhz_clk_clk_p => default_100mhz_clk_clk_p,

      addn_ui_clkout3_0              => addn_ui_clkout3_0,

      -- Connect the ICAP
      ICAP_0_i               => icap_0_i,  -- This is the input data to the ICAP primitive
      ICAP_0_o               => icap_0_o,
      ICAP_0_csib            => icap_0_csib,
      ICAP_0_rdwrb           => icap_0_rdwrb,
      
      ICAP_0_avail => ICAP_0_avail,
      
      -- Connect to memory
      -- ddr3_sdram_addr      => ddr3_sdram_addr,
      -- ddr3_sdram_ba        => ddr3_sdram_ba,
      -- ddr3_sdram_cas_n     => ddr3_sdram_cas_n,
      -- ddr3_sdram_ck_n      => ddr3_sdram_ck_n,
      -- ddr3_sdram_ck_p      => ddr3_sdram_ck_p,
      -- ddr3_sdram_cke       => ddr3_sdram_cke,
      -- ddr3_sdram_cs_n      => ddr3_sdram_cs_n,
      -- ddr3_sdram_dm        => ddr3_sdram_dm,
      -- ddr3_sdram_dq        => ddr3_sdram_dq,
      -- ddr3_sdram_dqs_n     => ddr3_sdram_dqs_n,
      -- ddr3_sdram_dqs_p     => ddr3_sdram_dqs_p,
      -- ddr3_sdram_odt       => ddr3_sdram_odt,
      -- ddr3_sdram_ras_n     => ddr3_sdram_ras_n,
      -- ddr3_sdram_reset_n   => ddr3_sdram_reset_n,
      -- ddr3_sdram_we_n      => ddr3_sdram_we_n,
      ddr4_sdram_act_n => ddr4_sdram_act_n,
      ddr4_sdram_adr => ddr4_sdram_adr,
      ddr4_sdram_ba => ddr4_sdram_ba,
      ddr4_sdram_bg => ddr4_sdram_bg,
      ddr4_sdram_ck_c => ddr4_sdram_ck_c,
      ddr4_sdram_ck_t => ddr4_sdram_ck_t,
      ddr4_sdram_cke => ddr4_sdram_cke,
      ddr4_sdram_cs_n => ddr4_sdram_cs_n, 
      ddr4_sdram_dm_n => ddr4_sdram_dm_n,
      ddr4_sdram_dq => ddr4_sdram_dq, 
      ddr4_sdram_dqs_c => ddr4_sdram_dqs_c, 
      ddr4_sdram_dqs_t => ddr4_sdram_dqs_t, 
      ddr4_sdram_odt => ddr4_sdram_odt,
      ddr4_sdram_reset_n => ddr4_sdram_reset_n, 


      -- Connect to Ethernet
      mdio_mdc_mdc         => mdio_mdc_mdc,
      mdio_mdc_mdio_io     => mdio_mdc_mdio_io,
      
      -- mii_col              => mii_col,
      -- mii_crs              => mii_crs,
      -- mii_rst_n            => mii_rst_n,
      -- mii_rx_clk           => mii_rx_clk,
      -- mii_rx_dv            => mii_rx_dv,
      -- mii_rx_er            => mii_rx_er,
      -- mii_rxd              => mii_rxd,
      -- mii_tx_clk           => mii_tx_clk,
      -- mii_tx_en            => mii_tx_en,
      -- mii_txd              => mii_txd,
      sgmii_lvds_rxn => sgmii_lvds_rxn,
      sgmii_lvds_rxp => sgmii_lvds_rxp,
      sgmii_lvds_txn => sgmii_lvds_txn,
      sgmii_lvds_txp => sgmii_lvds_txp,
      sgmii_phyclk_clk_n => sgmii_phyclk_clk_n,
      sgmii_phyclk_clk_p => sgmii_phyclk_clk_p,

      dummy_port_in => dummy_port_in,
      
      -- rs232_uart_rxd       => rs232_uart_rxd,
      -- rs232_uart_txd       => rs232_uart_txd
      rs232_uart_0_rxd => rs232_uart_0_rxd,
      rs232_uart_0_txd => rs232_uart_0_txd
      );
  

  -- -------------------------------------------------------------------------
  -- Virtual Sockets 
  -- -------------------------------------------------------------------------
  -- Virtual Socket 0: shift
  inst_shift : shift
    port map (
      en       => '0',
      clk      => addn_ui_clkout3_0,
      addr     => count_value(34 downto 23),
      data_out => shift_out
      );

  -- Virtual Socket 1: count
  inst_count : count
    port map (
      rst       => '0',
      clk       => addn_ui_clkout3_0,
      count_out => count_out
      );

  -- -- Instantiate ICAP primitive
  -- --
  -- ICAPE2_inst : ICAPE2
  --   generic map (
  --     DEVICE_ID  => X"3651093",         -- Device ID code for Kintex-7 XC7K325T-2FFG900C
  --     ICAP_WIDTH => "X32"               -- Specifies the input and output data width to be used with the ICAPE2.
  --     )
  --   port map (
  --     O          => icap_o,             -- 32-bit output: Configuration data output bus
  --     CLK        => addn_ui_clkout3_0,            -- 1-bit input: Clock Input
  --     CSIB       => icap_csib,          -- 1-bit input: Active-Low ICAP Enable
  --     I          => icap_i,             -- 32-bit input: Configuration data input bus
  --     RDWRB      => icap_rdwrb          -- 1-bit input: Read/Write Select input
  --     );


--   ICAPE3    : In order to incorporate this function into the design,
--    VHDL     : the following instance declaration needs to be placed
--  instance   : in the body of the design code.  The instance name
-- declaration : (ICAPE3_inst) and/or the port declarations after the
--    code     : "=>" declaration maybe changed to properly reference and
--             : connect this function to the design.  All inputs and outputs
--             : must be connected.

--   Library   : In addition to adding the instance declaration, a use
-- declaration : statement for the UNISIM.vcomponents library needs to be
--     for     : added before the entity declaration.  This library
--   Xilinx    : contains the component declarations for all Xilinx
-- primitives  : primitives and points to the models that will be used
--             : for simulation.

--  Copy the following two statements and paste them before the
--  Entity declaration, unless they already exist.

-- <-----Cut code below this line and paste into the architecture body---->

   -- ICAPE3: Internal Configuration Access Port
   --         Virtex UltraScale+
   -- Xilinx HDL Language Template, version 2019.1

   ICAPE3_inst : ICAPE3
   generic map (
      DEVICE_ID => X"03628093",      -- Specifies the pre-programmed Device ID value to be used for simulation
                                     -- purposes.
      ICAP_AUTO_SWITCH => "DISABLE", -- Enable switch ICAP using sync word
      SIM_CFG_FILE_NAME => "NONE"    -- Specifies the Raw Bitstream (RBT) file to be parsed by the simulation
                                     -- model
   )
   port map (
      AVAIL => ICAP_0_avail,     -- 1-bit output: Availability status of ICAP
      O => icap_0_o,             -- 32-bit output: Configuration data output bus
      -- PRDONE => PRDONE,   -- 1-bit output: Indicates completion of Partial Reconfiguration
      -- PRERROR => PRERROR, -- 1-bit output: Indicates Error during Partial Reconfiguration
      CLK => addn_ui_clkout3_0,         -- 1-bit input: Clock input
      CSIB => icap_0_csib,       -- 1-bit input: Active-Low ICAP enable
      I => icap_0_i,             -- 32-bit input: Configuration data input bus
      RDWRB => icap_0_rdwrb      -- 1-bit input: Read/Write Select input
   );

   -- End of ICAPE3_inst instantiation
				



  b_ila_icap            : block
    signal icap_csib_v  : std_logic_vector(0 downto 0);
    signal icap_rdwrb_v : std_logic_vector(0 downto 0);
    signal icap_status  : std_logic_vector(3 downto 0);
  begin
    icap_csib_v(0)  <= icap_0_csib;
    icap_rdwrb_v(0) <= icap_0_rdwrb;
    icap_status     <= icap_0_o(7 downto 4);

    i_ila_icap : ila_icap
      port map (
        clk          => addn_ui_clkout3_0,
        trig_out     => open,
        trig_out_ack => '1',

        probe0       => icap_0_i,
        probe1       => icap_status,
        probe2       => icap_csib_v,
        probe3       => icap_rdwrb_v
        );
  end block;

  -- -------------------------------------------------------------------------
  -- Misc
  -- -------------------------------------------------------------------------
  -- This counter provides an address for the shift Virtual Socket where it is used to address a BRAM
  p_count : process (addn_ui_clkout3_0)
  begin
    if rising_edge(addn_ui_clkout3_0) then
      if reset = '1' then
        count_value <= (others => '0');
      else
        count_value <= std_logic_vector(unsigned(count_value) + 1);
      end if;
    end if;
  end process;


  -- -------------------------------------------------------------------------
  -- Board Connections
  -- -------------------------------------------------------------------------


end rtl;





