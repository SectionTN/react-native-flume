package expo.modules.benchbaseline

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.facebook.drawee.generic.GenericDraweeHierarchyBuilder
import com.facebook.drawee.generic.RoundingParams
import com.facebook.drawee.drawable.ScalingUtils
import com.facebook.drawee.view.SimpleDraweeView
import com.facebook.drawee.backends.pipeline.Fresco
import com.facebook.imagepipeline.common.ResizeOptions
import com.facebook.imagepipeline.request.ImageRequestBuilder
import expo.modules.kotlin.AppContext
import expo.modules.kotlin.views.ExpoView
import kotlin.math.roundToInt

// The chat bench's reference: a plain RecyclerView whose rows repeat the chat cell's values, built in code.
class BenchBaselineView(context: Context, appContext: AppContext) : ExpoView(context, appContext) {
  // A getter, not a field: the super constructor calls requestLayout before subclass fields exist.
  override val shouldUseAndroidLayout: Boolean
    get() = true

  private val rowsAdapter = RowsAdapter()
  private val rowsLayout = LinearLayoutManager(context).apply { stackFromEnd = true }

  init {
    addView(
      RecyclerView(context).apply {
        layoutManager = rowsLayout
        adapter = rowsAdapter
      },
      LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT),
    )
  }

  fun setRows(rows: List<ChatRowRecord>) {
    rowsAdapter.submit(rows)
  }

  // The item at the top edge, as Flume's and FlatList's scrollToIndex place it.
  fun scrollToIndex(index: Int) {
    rowsLayout.scrollToPositionWithOffset(index, 0)
  }
}

private class RowsAdapter : RecyclerView.Adapter<RowHolder>() {
  private var rows: List<ChatRowRecord> = emptyList()

  init {
    setHasStableIds(true)
  }

  fun submit(next: List<ChatRowRecord>) {
    rows = next
    notifyDataSetChanged()
  }

  override fun getItemCount() = rows.size

  override fun getItemId(position: Int) = rows[position].id.hashCode().toLong()

  override fun getItemViewType(position: Int) = if (rows[position].kind == "voice") VOICE else MESSAGE

  override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) = RowHolder(parent.context, viewType == VOICE)

  override fun onBindViewHolder(holder: RowHolder, position: Int) = holder.bind(rows[position])

  companion object {
    const val MESSAGE = 0
    const val VOICE = 1
  }
}

private class RowHolder(context: Context, voice: Boolean) : RecyclerView.ViewHolder(LinearLayout(context)) {
  private val density = context.resources.displayMetrics.density
  private val avatarPx = dp(40)
  private val avatar = SimpleDraweeView(
    context,
    GenericDraweeHierarchyBuilder(context.resources)
      .setFadeDuration(0)
      .setActualImageScaleType(ScalingUtils.ScaleType.CENTER_CROP)
      .setRoundingParams(RoundingParams.asCircle())
      .build(),
  )
  private val text = label(context, 16f, TEXT)
  private val time = label(context, 12f, MUTED)

  init {
    val row = itemView as LinearLayout
    row.orientation = LinearLayout.HORIZONTAL
    row.gravity = Gravity.CENTER_VERTICAL
    // Yoga rounds the row edges, so 2 x 12 dp totals round(24 dp) rather than twice a truncated 12 dp.
    val vertical = (24 * density).roundToInt()
    row.setPadding(dp(12), dp(12), dp(12), vertical - dp(12))
    row.setBackgroundColor(Color.WHITE)
    row.layoutParams = RecyclerView.LayoutParams(RecyclerView.LayoutParams.MATCH_PARENT, RecyclerView.LayoutParams.WRAP_CONTENT)
    row.addView(avatar, LinearLayout.LayoutParams(avatarPx, avatarPx))
    val body = LinearLayout(context)
    if (voice) {
      body.orientation = LinearLayout.HORIZONTAL
      body.gravity = Gravity.CENTER_VERTICAL
      body.addView(label(context, 18f, ACCENT).apply { this.text = "▶" })
      val track = View(context).apply {
        background = GradientDrawable().apply {
          cornerRadius = 2f * density
          setColor(TRACK)
        }
      }
      body.addView(track, LinearLayout.LayoutParams(0, dp(4), 1f).apply { marginStart = dp(8) })
      body.addView(time, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { marginStart = dp(8) })
    } else {
      body.orientation = LinearLayout.VERTICAL
      body.addView(text)
      body.addView(time)
    }
    row.addView(body, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply { marginStart = dp(12) })
    val like = label(context, 14f, ACCENT).apply {
      this.text = "Like"
      typeface = Typeface.create(Typeface.DEFAULT, 600, false)
      setPadding(dp(8), dp(8), dp(8), dp(8))
    }
    row.addView(like, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply { marginStart = dp(12) })
  }

  fun bind(row: ChatRowRecord) {
    text.text = row.text
    time.text = if (row.kind == "voice") row.duration ?: "" else row.time
    val request = ImageRequestBuilder.newBuilderWithSource(Uri.parse(row.avatar))
      .setResizeOptions(ResizeOptions(avatarPx, avatarPx))
      .build()
    avatar.controller = Fresco.newDraweeControllerBuilder().setImageRequest(request).setOldController(avatar.controller).build()
  }

  private fun dp(value: Int) = (value * density).toInt()

  companion object {
    val TEXT = Color.parseColor("#111111")
    val MUTED = Color.parseColor("#666666")
    val ACCENT = Color.parseColor("#0066ff")
    val TRACK = Color.parseColor("#dddddd")

    fun label(context: Context, sizeSp: Float, color: Int) = TextView(context).apply {
      setTextSize(TypedValue.COMPLEX_UNIT_SP, sizeSp)
      setTextColor(color)
    }
  }
}
