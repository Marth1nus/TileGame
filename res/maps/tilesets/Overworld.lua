return {
  version = "1.10",
  luaversion = "5.1",
  tiledversion = "1.11.0",
  name = "Overworld",
  class = "",
  tilewidth = 16,
  tileheight = 16,
  spacing = 0,
  margin = 0,
  columns = 40,
  image = "../../images/gfx/Overworld.png",
  imagewidth = 640,
  imageheight = 576,
  objectalignment = "unspecified",
  tilerendersize = "tile",
  fillmode = "stretch",
  tileoffset = {
    x = 0,
    y = 0
  },
  grid = {
    orientation = "orthogonal",
    width = 16,
    height = 16
  },
  properties = {},
  wangsets = {
    {
      name = "outside-floor",
      class = "",
      tile = -1,
      wangsettype = "mixed",
      properties = {},
      colors = {
        {
          color = { 0, 255, 127 },
          name = "cut grass",
          class = "",
          probability = 1,
          tile = -1,
          properties = {}
        },
        {
          color = { 0, 170, 0 },
          name = "wild grass",
          class = "",
          probability = 1,
          tile = -1,
          properties = {}
        },
        {
          color = { 0, 0, 255 },
          name = "water",
          class = "",
          probability = 1,
          tile = -1,
          properties = {}
        }
      },
      wangtiles = {
        {
          wangid = { 2, 2, 2, 2, 2, 2, 2, 2 },
          tileid = 0
        },
        {
          wangid = { 2, 2, 2, 1, 2, 2, 2, 2 },
          tileid = 120
        },
        {
          wangid = { 2, 2, 2, 1, 1, 1, 2, 2 },
          tileid = 121
        },
        {
          wangid = { 2, 2, 2, 2, 2, 1, 2, 2 },
          tileid = 122
        },
        {
          wangid = { 2, 1, 1, 1, 2, 2, 2, 2 },
          tileid = 160
        },
        {
          wangid = { 1, 1, 1, 1, 1, 1, 1, 1 },
          tileid = 161
        },
        {
          wangid = { 2, 2, 2, 2, 2, 1, 1, 1 },
          tileid = 162
        },
        {
          wangid = { 2, 1, 2, 2, 2, 2, 2, 2 },
          tileid = 200
        },
        {
          wangid = { 1, 1, 2, 2, 2, 2, 2, 1 },
          tileid = 201
        },
        {
          wangid = { 2, 2, 2, 2, 2, 2, 2, 1 },
          tileid = 202
        },
        {
          wangid = { 1, 1, 2, 2, 2, 1, 1, 1 },
          tileid = 240
        },
        {
          wangid = { 1, 1, 1, 1, 2, 2, 2, 1 },
          tileid = 241
        },
        {
          wangid = { 2, 2, 2, 3, 2, 2, 2, 2 },
          tileid = 242
        },
        {
          wangid = { 2, 2, 2, 3, 3, 3, 2, 2 },
          tileid = 243
        },
        {
          wangid = { 2, 2, 2, 2, 2, 3, 2, 2 },
          tileid = 244
        },
        {
          wangid = { 1, 1, 1, 3, 1, 1, 1, 1 },
          tileid = 255
        },
        {
          wangid = { 1, 1, 1, 3, 3, 3, 1, 1 },
          tileid = 256
        },
        {
          wangid = { 1, 1, 1, 1, 1, 3, 1, 1 },
          tileid = 257
        },
        {
          wangid = { 2, 2, 2, 1, 1, 1, 1, 1 },
          tileid = 280
        },
        {
          wangid = { 2, 1, 1, 1, 1, 1, 2, 2 },
          tileid = 281
        },
        {
          wangid = { 2, 3, 3, 3, 2, 2, 2, 2 },
          tileid = 282
        },
        {
          wangid = { 3, 3, 3, 3, 3, 3, 3, 3 },
          tileid = 283
        },
        {
          wangid = { 2, 2, 2, 2, 2, 3, 3, 3 },
          tileid = 284
        },
        {
          wangid = { 1, 3, 3, 3, 1, 1, 1, 1 },
          tileid = 295
        },
        {
          wangid = { 1, 1, 1, 1, 1, 3, 3, 3 },
          tileid = 297
        },
        {
          wangid = { 2, 3, 2, 2, 2, 2, 2, 2 },
          tileid = 322
        },
        {
          wangid = { 3, 3, 2, 2, 2, 2, 2, 3 },
          tileid = 323
        },
        {
          wangid = { 2, 2, 2, 2, 2, 2, 2, 3 },
          tileid = 324
        },
        {
          wangid = { 1, 3, 1, 1, 1, 1, 1, 1 },
          tileid = 335
        },
        {
          wangid = { 3, 3, 1, 1, 1, 1, 1, 3 },
          tileid = 336
        },
        {
          wangid = { 1, 1, 1, 1, 1, 1, 1, 3 },
          tileid = 337
        },
        {
          wangid = { 3, 3, 2, 2, 2, 3, 3, 3 },
          tileid = 362
        },
        {
          wangid = { 3, 3, 3, 3, 2, 2, 2, 3 },
          tileid = 363
        },
        {
          wangid = { 3, 3, 1, 1, 1, 3, 3, 3 },
          tileid = 375
        },
        {
          wangid = { 3, 3, 3, 3, 1, 1, 1, 3 },
          tileid = 376
        },
        {
          wangid = { 2, 2, 2, 3, 3, 3, 3, 3 },
          tileid = 402
        },
        {
          wangid = { 2, 3, 3, 3, 3, 3, 2, 2 },
          tileid = 403
        },
        {
          wangid = { 1, 1, 1, 3, 3, 3, 3, 3 },
          tileid = 415
        },
        {
          wangid = { 1, 3, 3, 3, 3, 3, 1, 1 },
          tileid = 416
        },
        {
          wangid = { 1, 1, 2, 2, 2, 3, 3, 3 },
          tileid = 528
        },
        {
          wangid = { 1, 3, 3, 3, 2, 2, 2, 1 },
          tileid = 529
        },
        {
          wangid = { 3, 3, 2, 2, 2, 1, 1, 3 },
          tileid = 530
        },
        {
          wangid = { 3, 3, 1, 1, 2, 2, 2, 3 },
          tileid = 531
        },
        {
          wangid = { 2, 2, 2, 1, 1, 3, 3, 3 },
          tileid = 568
        },
        {
          wangid = { 2, 3, 3, 3, 1, 1, 2, 2 },
          tileid = 569
        },
        {
          wangid = { 2, 2, 2, 3, 3, 3, 1, 1 },
          tileid = 570
        },
        {
          wangid = { 2, 1, 1, 3, 3, 3, 2, 2 },
          tileid = 571
        }
      }
    },
    {
      name = "surface-decor",
      class = "",
      tile = -1,
      wangsettype = "edge",
      properties = {},
      colors = {
        {
          color = { 170, 85, 0 },
          name = "fense",
          class = "",
          probability = 1,
          tile = -1,
          properties = {}
        },
        {
          color = { 0, 255, 0 },
          name = "bushes",
          class = "",
          probability = 1,
          tile = -1,
          properties = {}
        }
      },
      wangtiles = {
        {
          wangid = { 0, 0, 2, 0, 2, 0, 0, 0 },
          tileid = 521
        },
        {
          wangid = { 0, 0, 2, 0, 0, 0, 2, 0 },
          tileid = 522
        },
        {
          wangid = { 0, 0, 0, 0, 2, 0, 2, 0 },
          tileid = 523
        },
        {
          wangid = { 0, 0, 0, 0, 2, 0, 0, 0 },
          tileid = 560
        },
        {
          wangid = { 2, 0, 0, 0, 2, 0, 0, 0 },
          tileid = 561
        },
        {
          wangid = { 2, 0, 2, 0, 2, 0, 2, 0 },
          tileid = 562
        },
        {
          wangid = { 2, 0, 0, 0, 2, 0, 0, 0 },
          tileid = 563
        },
        {
          wangid = { 2, 0, 0, 0, 0, 0, 0, 0 },
          tileid = 600
        },
        {
          wangid = { 2, 0, 2, 0, 0, 0, 0, 0 },
          tileid = 601
        },
        {
          wangid = { 0, 0, 2, 0, 0, 0, 2, 0 },
          tileid = 602
        },
        {
          wangid = { 2, 0, 0, 0, 0, 0, 2, 0 },
          tileid = 603
        },
        {
          wangid = { 0, 0, 2, 0, 0, 0, 0, 0 },
          tileid = 640
        },
        {
          wangid = { 0, 0, 0, 0, 0, 0, 2, 0 },
          tileid = 641
        },
        {
          wangid = { 0, 0, 0, 0, 1, 0, 0, 0 },
          tileid = 680
        },
        {
          wangid = { 0, 0, 1, 0, 1, 0, 0, 0 },
          tileid = 682
        },
        {
          wangid = { 0, 0, 0, 0, 1, 0, 1, 0 },
          tileid = 683
        },
        {
          wangid = { 0, 0, 1, 0, 0, 0, 1, 0 },
          tileid = 684
        },
        {
          wangid = { 1, 0, 0, 0, 0, 0, 0, 0 },
          tileid = 720
        },
        {
          wangid = { 1, 0, 1, 0, 0, 0, 0, 0 },
          tileid = 722
        },
        {
          wangid = { 1, 0, 0, 0, 0, 0, 1, 0 },
          tileid = 723
        },
        {
          wangid = { 1, 0, 0, 0, 1, 0, 0, 0 },
          tileid = 724
        },
        {
          wangid = { 0, 0, 1, 0, 0, 0, 0, 0 },
          tileid = 760
        },
        {
          wangid = { 0, 0, 0, 0, 0, 0, 1, 0 },
          tileid = 761
        }
      }
    }
  },
  tilecount = 1440,
  tiles = {}
}
