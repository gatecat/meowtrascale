# rows, height
def get_size(tt):
    if tt in ("CLEM", "CLEM_R", "CLEL_L", "CLEL_R"):
        return (16, 1)
    if tt == "INT":
        return (76, 1)
    if tt == "DSP":
        return (8, 5)
    if tt == "BRAM":
        return (6, 5)
    if tt in ("URAM_URAM_FT", "URAM_URAM_DELAY_FT"):
        return (9, 15)
    if tt in ("HPIO_L", "HPIO_R"):
        return (8, 30)
    if tt in ("XIPHY_BYTE_L", "XIPHY_BYTE_R"):
        return (4, 15)
    if tt in ("HDIO_BOT_RIGHT", "HDIO_TOP_RIGHT", "CFGIO_IOB20", "AMS"):
        return (10, 30)
    if tt in ("GTH_QUAD_RIGHT", "GTY_L", "GTY_R"):
        return (8, 60)
    if tt in ( "PCIE4_PCIE4_FT", "CFG_CONFIG", "ILKN_ILKN_FT", "CMAC", "PCIE4C_PCIE4C_FT"):
        return (10, 60)
    if tt in ("INT_INTF_L_IO", "INT_INTF_R_IO", "INT_INTF_R_TERM_GT",
                "INT_INTF_R_PCIE4", "INT_INTF_L_PCIE4", "INT_INTF_L_CMT",
                "INT_INTF_LEFT_TERM_IO_FT", "INT_INTF_L_TERM_GT"):
        return (4, 1)
    if tt == "BLI_BLI_FT":
        return (8, 15)
    if tt in ("CMT_L", "CMT_RIGHT", "CMT_LEFT_H"):
        return (12, 60)
