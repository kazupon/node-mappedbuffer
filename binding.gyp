{
  'target_defaults': {
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ]
      },
      'Release': {
        'defines': [ 'NDEBUG' ]
      }
    }
  },
  'targets': [
    {
      'target_name': 'mappedbuffer',
      'sources': [
        'src/mappedbuffer.cc'
      ]
    }
  ]
}
